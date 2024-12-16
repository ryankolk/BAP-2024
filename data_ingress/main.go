package main

import (
	"bytes"
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"time"

	"log/slog"

	influxdb2 "github.com/influxdata/influxdb-client-go/v2"
	"github.com/tarm/serial"
)

const POLYNOMIAL uint32 = 0x1021

func checksumCalculator(data []byte, length int) uint16 {
	var crc uint32 = 0xFFFF

	for i := 0; i < length; i++ {
		crc ^= uint32(data[i]) << 8

		for bit := 0; bit < 8; bit++ {
			if (crc & 0x8000) != 0 {
				crc = (crc << 1) ^ POLYNOMIAL
			} else {
				crc <<= 1
			}
		}
	}

	return uint16(crc & 0xFFFF)
}

func readSerialData(s *serial.Port) ([]byte, error) {
	var message []byte
	response := make([]byte, 256)
	for {
		n, err := s.Read(response)
		if err != nil {
			if err == io.EOF {
				return message, nil
			}
			return message, fmt.Errorf("failed to read from serial port: %w", err)
		}
		
		if n == 0{
			return message, nil
		}
		message = append(message, response[:n]...)
		
	}
}

func main() {
	config, err := readConfig()

	if err != nil {
		panic(err)
	}
	fmt.Printf("Config: %+v\n", config)
	slog.SetLogLoggerLevel(getLogLevel(config.Log.Level))

	client := influxdb2.NewClient(config.Influx.Url, config.Influx.Token)
	defer client.Close()
	writeAPI := client.WriteAPIBlocking(config.Influx.Org, config.Influx.Bucket)

	// Serial Port Configuration
	c := &serial.Config{Name: config.Serial.Port, Baud: config.Serial.BaudRate, ReadTimeout: time.Millisecond * 100} // Added read timeout
	s, err := serial.OpenPort(c)
	if err != nil {
		slog.Error("Failed to open serial port: %v", err)
	}
	defer s.Close()

	messageQueue := make([]Message, 0)

	for {
		slog.Info("Sending start byte 0x25 to request data")
		_, err = s.Write([]byte{0x25})
		if err != nil {
			slog.Error("Failed to send start byte: %v", err)
			continue
		}
		slog.Info("Start byte sent successfully")

		var message []byte
		time.Sleep(10 * time.Millisecond)

		message, err := readSerialData(s)
		if err != nil {
			slog.Error("Failed to read from serial port: %v", err)
			continue
		}
		

		if len(message) > 0 {
			slog.Info("Received %d bytes: % X", len(message), message)
			// If the message has enough data to contain length and checksum
			if len(message) >= 2 {
				length := int(message[0]) | int(message[1])<<8
				slog.Debug("Expecting data of length %d", length)

				if len(message) >= length+4 {
					data := message[4 : 4+length]
					expectedChecksum := uint16(message[2]) | uint16(message[3])<<8
					slog.Debug("Checksum for received data: 0x%04X", expectedChecksum)
					calculatedChecksum := checksumCalculator(data, len(data)) & 0xFFFF

					if calculatedChecksum != expectedChecksum {
						slog.Error("Checksum mismatch: expected 0x%04X, got 0x%04X", expectedChecksum, calculatedChecksum)
						// Send resend byte 0x19
						slog.Info("Sending resend byte 0x19")
						_, err = s.Write([]byte{0x19})
						if err != nil {
							slog.Error("Failed to send resend byte: %v", err)
						} else {
							slog.Debug("Resend byte sent successfully")
						}
					} else {
						// Store valid data in the queue
						for i := 0; i+56 <= len(data); i += 56 {
							var outputData OutputData
							headerSize := binary.Size(MessageHeader{})
							outputDataSize := binary.Size(OutputData{})
							slog.Debug("Received data of length %d", headerSize+outputDataSize)
							if len(data[i:]) >= headerSize+outputDataSize {
								headerBytes := data[i : i+headerSize]
								outputDataBytes := data[i+headerSize : i+headerSize+outputDataSize]

								var messageHeader MessageHeader
								err := binary.Read(bytes.NewReader(headerBytes), binary.LittleEndian, &messageHeader)
								if err != nil {
									slog.Error("Failed to decode message header: %v", err)
								} else {
									slog.Info("Decoded message header: %+v", messageHeader)
									err = binary.Read(bytes.NewReader(outputDataBytes), binary.LittleEndian, &outputData)
									if err != nil {
										slog.Error("Failed to decode output data: %v", err)
									} else {
										slog.Info("Decoded Output Data: %+v", outputData)

										message := Message{
											MessageHeader: messageHeader,
											Data:          &outputData,
										}
                    if(messageHeader.Timestamp < 1000) {
											slog.Error("Timestamp is less than 1000")
                      continue
                    }

										messageQueue = append(messageQueue, message)
									}
								}
							}
						}
						
						// Acknowledge with byte 0x07
						slog.Debug("Checksum matches, sending ACK byte 0x07")
						_, err = s.Write([]byte{0x07})
						if err != nil {
							slog.Error("Failed to send ACK byte: %v", err)
						} else {
							slog.Info("ACK byte sent successfully")
						}
					}
				}
			}
		} else {
			slog.Error("No response received from ESP32 after sending start byte")
		}

		s.Flush()

		// If message queue is large enough, send batch data
		if len(messageQueue) >= 2 {
			go func(queue []Message) {
				// Write data to InfluxDB
				for _, msg := range queue {
					p := influxdb2.NewPoint(
						"measurement",
						map[string]string{"id": fmt.Sprintf("%d", msg.MessageHeader.ID+1)},
						map[string]interface{}{
							"avgDb":               msg.Data.MicrophoneData.AvgDb,
							"peakFrequency":       msg.Data.MicrophoneData.PeakFrequency,
							"zeroCrossingsCount":  msg.Data.MicrophoneData.ZeroCrossingCount,
							"roll":                msg.Data.AccelerometerData.Roll,
							"pitch":               msg.Data.AccelerometerData.Pitch,
							"yaw":                 msg.Data.AccelerometerData.Yaw,
						},
						time.Unix(int64(msg.MessageHeader.Timestamp), int64(msg.MessageHeader.TimestampUs)*int64(time.Microsecond)),
					)
					if err := writeAPI.WritePoint(context.Background(), p); err != nil {
						fmt.Printf("Error writing point to InfluxDB: %v\n", err)
					}
					for _, ble := range msg.Data.BleData {
						if ble.DeviceName != 0 {
							p := influxdb2.NewPoint(
								"BLEData",
								map[string]string{"id": fmt.Sprintf("%d", msg.MessageHeader.ID+1), "device": fmt.Sprintf("%d", ble.DeviceName+1)},
								map[string]interface{}{
									"rssi": ble.Rssi,
								},
								time.Unix(int64(msg.MessageHeader.Timestamp), int64(msg.MessageHeader.TimestampUs)*int64(time.Microsecond)),
							)
							if err := writeAPI.WritePoint(context.Background(), p); err != nil {
								fmt.Printf("Error writing point to InfluxDB: %v\n", err)
							}
						}
					}
				}
			}(messageQueue)
			messageQueue = messageQueue[:0]
		}
	}
}

func syncTime(s *serial.Port) error {
		now := time.Now()
		seconds := uint32(now.Unix())
		microseconds := uint32(now.UnixMicro() % 1e6)

		timeBytes := make([]byte, 8)
		binary.LittleEndian.PutUint32(timeBytes[0:4], seconds)
		binary.LittleEndian.PutUint32(timeBytes[4:8], microseconds)

		slog.Info("Sending current time with header and checksum")
		_, err := s.Write([]byte{0x11})
		if err != nil {
			return err
		}
		_, err = s.Write(timeBytes)
		if err != nil {
			return err
		}
		slog.Info("Time synchronization data sent successfully")

		// Wait for ACK from node
		message, err := readSerialData(s)
		if err != nil {
			return err
		}

		if len(message) == 1 && message[0] == 0x07 {
			slog.Info("Node acknowledged time synchronization data with ACK byte 0x07")
		} else {
			return fmt.Errorf("Did not receive expected ACK byte. Received: % X\n", message)
		}

		return nil
}
