#include <rtl_tcp_client.h>
#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/smgui.h>
#include <gui/style.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "readonly_rtltcp_source",
    /* Description:     */ "Readonly RTL-TCP source module for SDR++",
    /* Author:          */ "BUSH22",
    /* Version:         */ 1, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class ReadonlyRTLTCPSourceModule : public ModuleManager::Instance {
public:
    ReadonlyRTLTCPSourceModule(std::string name) {
        this->name = name;

        // Define samplerates
        samplerates.define(250e3, "250KHz", 250e3);
        samplerates.define(1.024e6, "1.024MHz", 1.024e6);
        samplerates.define(1.536e6, "1.536MHz", 1.536e6);
        samplerates.define(1.792e6, "1.792MHz", 1.792e6);
        samplerates.define(1.92e6, "1.92MHz", 1.92e6);
        samplerates.define(2.048e6, "2.048MHz", 2.048e6);
        samplerates.define(2.16e6, "2.16MHz", 2.16e6);
        samplerates.define(2.4e6, "2.4MHz", 2.4e6);
        samplerates.define(2.56e6, "2.56MHz", 2.56e6);
        samplerates.define(2.88e6, "2.88MHz", 2.88e6);
        samplerates.define(3.2e6, "3.2MHz", 3.2e6);

        // Define direct sampling modes
        directSamplingModes.define(0, "Disabled", 0);
        directSamplingModes.define(1, "I branch", 1);
        directSamplingModes.define(2, "Q branch", 2);

        // Select the default samplerate instead of id 0
        srId = samplerates.valueId(2.4e6);

        // Load config
        config.acquire();
        if (config.conf.contains("host")) {
            std::string hostStr = config.conf["host"];
            strcpy(ip, hostStr.c_str());
        }
        if (config.conf.contains("port")) {
            port = config.conf["port"];
        }
        if (config.conf.contains("sampleRate")) {
            double sr = config.conf["sampleRate"];
            if (samplerates.keyExists(sr)) { srId = samplerates.keyId(sr); }
        }
        config.release();

        // Update samplerate
        sampleRate = samplerates[srId];

        // Register source
        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("RTL-TCP", &handler);
    }

    ~ReadonlyRTLTCPSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("RTL-TCP");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    static void menuSelected(void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("ReadonlyRTLTCPSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;
        flog::info("ReadonlyRTLTCPSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;
        if (_this->running) { return; }
        
        // Connect to the server
        try {
            _this->client = rtltcp::connect(&_this->stream, _this->ip, _this->port);
        }
        catch (const std::exception& e) {
            flog::error("Could connect to RTL-TCP server: {}", e.what());
            return;
        }
        

        _this->running = true;
        flog::info("ReadonlyRTLTCPSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->client->close();
        _this->running = false;
        flog::info("ReadonlyRTLTCPSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;
        _this->freq = freq;
        flog::info("Module is not tunable! The frequency here is just for convenience");
    }

    static void menuHandler(void* ctx) {
        ReadonlyRTLTCPSourceModule* _this = (ReadonlyRTLTCPSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }

        if (SmGui::InputText(CONCAT("##_ip_select_", _this->name), _this->ip, 1024)) {
            config.acquire();
            config.conf["host"] = std::string(_this->ip);
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_port_select_", _this->name), &_this->port, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }

        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_rtltcp_sr_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            config.acquire();
            config.conf["sampleRate"] = _this->sampleRate;
            config.release(true);
        }

        if (_this->running) { SmGui::EndDisabled(); }
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    std::shared_ptr<rtltcp::Client> client;
    bool running = false;
    double freq;

    char ip[1024] = "localhost";
    int port = 1234;
    int srId = 0;
    int directSamplingId = 0;
    int ppm = 0;
    int gain = 0;
    bool biasTee = false;
    bool offsetTuning = false;
    bool rtlAGC = false;
    bool tunerAGC = false;

    OptionList<double, double> samplerates;
    OptionList<int, int> directSamplingModes;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/rtl_tcp_config.json");
    config.load(json({}));
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new ReadonlyRTLTCPSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (ReadonlyRTLTCPSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}