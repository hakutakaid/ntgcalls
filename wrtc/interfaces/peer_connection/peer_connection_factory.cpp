//
// Created by Laky64 on 16/08/2023.
//

#include "peer_connection_factory.hpp"

namespace wrtc {
    std::mutex PeerConnectionFactory::_mutex{};
    int PeerConnectionFactory::_references = 0;
    rtc::scoped_refptr<PeerConnectionFactory> PeerConnectionFactory::_default = nullptr;

    PeerConnectionFactory::PeerConnectionFactory() {
        _workerThread = rtc::Thread::Create();
        _workerThread->SetName("worker_thread", nullptr);
        RTC_CHECK(_workerThread->Start()) << "Failed to start thread";

        _signalingThread = rtc::Thread::Create();
        _signalingThread->SetName("signaling_thread", nullptr);
        RTC_CHECK(_signalingThread->Start()) << "Failed to start thread";

        _networkThread = rtc::Thread::CreateWithSocketServer();
        _networkThread->SetName("network_thread", nullptr);
        RTC_CHECK(_networkThread->Start()) << "Failed to start thread";
        if (!_audioDeviceModule) {
            _taskQueueFactory = webrtc::CreateDefaultTaskQueueFactory();
            _workerThread->BlockingCall([=] { CreateAudioDeviceModule_w(); });
        }

        // TODO: @dadadani Add intel video encoder-decoder when available
        if (!_factory) {
            _factory = webrtc::CreatePeerConnectionFactory(
                    _networkThread.get(), _workerThread.get(), _signalingThread.get(),
                    _audioDeviceModule,
                    webrtc::CreateBuiltinAudioEncoderFactory(),
                    webrtc::CreateBuiltinAudioDecoderFactory(),
                    webrtc::CreateBuiltinVideoEncoderFactory(),
                    webrtc::CreateBuiltinVideoDecoderFactory(),
                    nullptr, nullptr);
        }
    }

    PeerConnectionFactory::~PeerConnectionFactory() {
        // TODO: @Laky-64 Add microphone/screen capture
        /*_workerThread->BlockingCall([&] {
            audio_device_impl_ = nullptr;
            video_device_impl_ = nullptr;
        });*/
        _factory = nullptr;

        if (_audioDeviceModule) {
            _workerThread->BlockingCall([this] { DestroyAudioDeviceModule_w(); });
        }

        _workerThread->Stop();
        _signalingThread->Stop();
        _networkThread->Stop();
    }

    void PeerConnectionFactory::CreateAudioDeviceModule_w() {
        if (!_audioDeviceModule)
            _audioDeviceModule = webrtc::AudioDeviceModule::Create(
                    webrtc::AudioDeviceModule::kPlatformDefaultAudio,
                    _taskQueueFactory.get());
    }

    void PeerConnectionFactory::DestroyAudioDeviceModule_w() {
        if (_audioDeviceModule)
            _audioDeviceModule = nullptr;
    }

    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> PeerConnectionFactory::factory() {
        return _factory;
    }

    rtc::scoped_refptr<PeerConnectionFactory> PeerConnectionFactory::GetOrCreateDefault() {
        _mutex.lock();
        _references++;
        if (_references == 1) {
            rtc::InitializeSSL();
            _default = rtc::scoped_refptr<PeerConnectionFactory>(new rtc::RefCountedObject<PeerConnectionFactory>());
        }
        _mutex.unlock();
        return _default;
    }

    void PeerConnectionFactory::UnRef() {
        _mutex.lock();
        _references--;
        if (!_references) {
            rtc::CleanupSSL();
            _default = nullptr;
        }
        _mutex.unlock();
    }
} // wrtc