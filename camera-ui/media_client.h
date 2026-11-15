#ifndef MEDIA_CLIENT_H
#define MEDIA_CLIENT_H

#include <memory>

class MediaClient
{
public:
    MediaClient();
    ~MediaClient();

    MediaClient(const MediaClient &) = delete;
    MediaClient &operator=(const MediaClient &) = delete;

    bool startCapture();
    bool stopCapture();

private:
    bool call(const char *method);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif
