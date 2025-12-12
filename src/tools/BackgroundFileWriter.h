#ifndef __BACKGROUND_FILE_WRITER_H
#define __BACKGROUND_FILE_WRITER_H

#include <Arduino.h>
#include <SdFat.h>

#include "MyMutex.h"

class CBackgroundFileWriter {
  private:
    SdFs* m_fs = nullptr;
    MyMutex* m_sdMutex = nullptr;
    QueueHandle_t m_queue = nullptr;
    TaskHandle_t m_task = nullptr;

    struct WriteJob {
        const char* fileName;
        uint8_t* data;
        size_t len;

        WriteJob(const char* fileName, const uint8_t* data, size_t len);
        ~WriteJob();
    };

  public:
    void setup(SdFs* sdFs, MyMutex* sdMutex, int queueDepth = 32);
    bool write(const char* logFileName, const uint8_t* output, size_t len);

  private:
    void startWriterTask();
    static void staticWriterTask(void* arg);
    void writerTask();
    void writeJobContents(WriteJob* job);
};


extern CBackgroundFileWriter BackgroundFileWriter; // Global singleton

#endif