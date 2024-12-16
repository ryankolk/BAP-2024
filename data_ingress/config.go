package main

import (
	"log/slog"
	"os"

	"gopkg.in/yaml.v3"
)

// type YAMLFile struct {
// 	Config Config `yaml:"config"`
// }
type Config struct {
	Influx struct {
		Bucket     string `yaml:"bucket"`
		Org 			 string `yaml:"org"`
		Token      string `yaml:"token"`
		Url        string `yaml:"url"`
	}
	Serial struct {
		BaudRate   int    `yaml:"baud_rate"`
		Port       string `yaml:"port"`
	}
	Log struct {
		Level  string `yaml:"level"`
	}
}
var data = `
bucket: "test"
`
var logLevels = map[string]slog.Level{
	"DEBUG": slog.LevelDebug,
	"INFO":  slog.LevelInfo,
	"WARN":  slog.LevelWarn,
	"ERROR": slog.LevelError,
}

func getLogLevel(level string) slog.Level {
	if _, ok := logLevels[level]; !ok {
		slog.Error("Invalid log level, defaulting to INFO")
		return slog.LevelInfo
	}
	return logLevels[level]
}


func readConfig() (*Config, error) {
	config := Config{}
	cfgFile, err := os.ReadFile("./config.yaml")
	if err != nil {
			return nil, err
	}
	err = yaml.Unmarshal(cfgFile, &config)
	return &config, err
}