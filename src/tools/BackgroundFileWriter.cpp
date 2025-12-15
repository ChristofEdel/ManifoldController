#include "BackgroundFileWriter.h"

////////////////////////////////////////////////////////////////////////////////////////////
//
// WriteJob - a set of data that will be written to a file
//

// Create a job to write a buffer to a file
// the data is copied into the job's own buffer, but we do NOT copy the fileName
// so we rely on the sender to manage the memory of fileName
CBackgroundFileWriter::WriteJob::WriteJob(const char* fileName, const uint8_t* data, size_t len)
{
    // We do NOT own fileName, we rely on the sender to manage its memory
    this->fileName = fileName,
    this->len = len;
    this->data = (uint8_t*)malloc(len);
    memcpy(this->data, data, len);
}

// Destructor - free the data buffer
// NOTE - we do not free fileName, as we do not own it
CBackgroundFileWriter::WriteJob::~WriteJob()
{
    // DO NOT FREE fileName, it's not allocated here, we rely on
    // the sender to manage its memory
    if (data) free(data);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// CBackgroundFileWriter - a background file writer using a queue and a dedicated task
//

// Setup the background file writer to use a particular SdFs and mutex, 
// and create the queue
void CBackgroundFileWriter::setup(SdFs *sdFs, MyMutex *sdMutex, int queueDepth /* = 10 */)
{
    m_sdMutex = sdMutex;
    m_fs = sdFs;
    m_queue = xQueueCreate(queueDepth, sizeof(WriteJob*));
    startWriterTask();
}

// Write to the file in the background
bool CBackgroundFileWriter::write(
    const char* logFileName,
    const uint8_t* output,
    size_t len
)
{
    // Initialisation check
    if (!m_queue || !m_fs || !m_sdMutex) return false;
    WriteJob* job = new WriteJob(logFileName, output, (size_t)len);

    if (xQueueSend(m_queue, &job, 0) != pdPASS) {
        delete job;
        return false;
    }
    return true;
}


void CBackgroundFileWriter::startWriterTask()
{
    if (m_task) return;

    BaseType_t ok = xTaskCreate(
        staticWriterTask,
        "BgFileWriter",
        4096,
        this,   // argument passed to task
        1,
        &m_task
    );
    if (ok != pdPASS) m_task = nullptr;
}

void CBackgroundFileWriter::staticWriterTask(void* arg) {
    static_cast<CBackgroundFileWriter*>(arg)->writerTask();
}

void CBackgroundFileWriter::writerTask()
{
    for (;;) {
        WriteJob* job = nullptr;
        if (xQueueReceive(m_queue, &job, portMAX_DELAY) != pdPASS) continue;
        if (!job) continue;
        this->writeJobContents(job);
        delete job;
    }
}

void CBackgroundFileWriter::writeJobContents(WriteJob *job) {
    if (this->m_sdMutex->lock(__PRETTY_FUNCTION__)) {
        FsFile file = m_fs->open(job->fileName, FILE_WRITE);
        if (!file) {
            // MyDebugLog.print("Unable to open log file ");
            // MyDebugLog.print(this->m_logFileName);
            // MyDebugLog.print(". Stopping file logging");
            // this->m_logToSdCard = false;
            return;
        }
        file.write(job->data, job->len); 
        file.close();
        this->m_sdMutex->unlock();
    }
}


CBackgroundFileWriter BackgroundFileWriter;
