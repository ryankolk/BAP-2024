package main

type MessageHeader struct {
	Type        uint32  // message_type_t type
	ID          uint8  // uint8_t id
  Buffer      uint16  // uint8_t id
  Buffer2     uint8  // uint8_t id
	Timestamp   uint32 // unsigned long timestamp
	TimestampUs uint32 // unsigned long timestamp_us
}

type MessageSyncRequest struct {
	// Define the fields of message_sync_request_t here
}

type MessageSyncResponse struct {
	// Define the fields of message_sync_response_t here
}

type MessageGeneric struct {
	// Define the fields of message_generic_t here
}

type MicrophoneData struct {
	AvgDb uint16
	PeakFrequency uint16
	ZeroCrossingCount uint16	// Define the fields of MicrophoneData here
	Unused uint16
}

type AccelerometerData struct {
	Roll float32
	Pitch float32
	Yaw float32
}

type DeviceRSSI struct {
	DeviceName uint8
	Rssi       uint8
}

type OutputData struct {
	MicrophoneData   MicrophoneData
	AccelerometerData AccelerometerData
	BleData          [10]DeviceRSSI
}

type Message struct {
	MessageHeader MessageHeader
	// SyncRequest   *MessageSyncRequest
	// SyncResponse  *MessageSyncResponse
	// Message       *MessageGeneric
	Data          *OutputData
}
