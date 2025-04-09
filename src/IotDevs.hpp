#pragma once
#include <CRC32.h>

#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "PubSub.hpp"
#include "model/settings/Settings.hpp"

typedef struct
{
    char device_id[80];
} DeviceStatus;

class DeviceCommand {
   public:
    int action;  // enum class supportedActions
    char device_id[80];
};

class RGB {
   private:
    uint8_t red, green, blue;

   public:
    RGB(uint8_t red, uint8_t green, uint8_t blue)
        : red(red), green(green), blue(blue) {}

    std::array<uint8_t, 3> asArray() const { return {this->red, this->green, this->blue}; }
    std::vector<uint8_t> asVector() const { return {this->red, this->green, this->blue}; }

    uint8_t getRed() const { return this->red; }
    void setRed(uint8_t red) { this->red = red; }

    uint8_t getGreen() const { return this->green; }
    void setGreen(uint8_t green) { this->green = green; }

    uint8_t getBlue() const { return this->blue; }
    void setBlue(uint8_t blue) { this->blue = blue; }
};

class IoTDevice {
   private:
    uint32_t _checksum;

   protected:
    std::string id;
    std::string name;
    bool state;

   public:
    IoTDevice(std::string id, std::string name)
        : id(id), name(name), state(false), _checksum(0) {}
    virtual ~IoTDevice() = default;

    virtual void describe() {};

    uint32_t getChecksum() { return this->_checksum; };
    void setChecksum(uint32_t val) { this->_checksum = val; };

    std::string getId() const { return this->id; }

    std::string getName() const { return this->name; }

    bool getState() const { return this->state; }
    void setState(bool state) { this->state = state; }

    enum class supportedActions {
        setState = 1
    };
};

class Shutter : public IoTDevice {
   public:
    int height;

    Shutter(std::string id, std::string name)
        : IoTDevice(id, name), height(0) {}

    enum class supportedActions {
        setState = 1,
        setHeight
    };
};

class Light : public IoTDevice {
   public:
    Light(std::string id, std::string name)
        : IoTDevice(id, name) {}
};

class DimmLight : public Light {
   public:
    int brightness;

    DimmLight(std::string id, std::string name)
        : Light(id, name), brightness(0) {}

    enum class supportedActions {
        setState = 1,
        setBrightness
    };
};

class ColorLight : public DimmLight {
   public:
    RGB color;

    ColorLight(std::string id, std::string name)
        : DimmLight(id, name), color(0, 0, 0) {}

    enum class supportedActions {
        setState = 1,
        setBrightness,
        setColor
    };
};

class Media : public IoTDevice {
   public:
    Media(std::string id, std::string name)
        : IoTDevice(id, name) {}
};

class Climate : public IoTDevice {
   public:
    Climate(std::string id, std::string name)
        : IoTDevice(id, name) {}

    enum class supportedActions {
        setState = 0,
        setTemp
    };
};

class Sensor : public IoTDevice {
   public:
    std::string unit;
    std::string value;

    Sensor(std::string id, std::string name)
        : IoTDevice(id, name), unit(""), value("") {}
};

class IoTDeviceFactory {
   public:
    static IoTDevice* create(const std::string& type, std::string id, const std::string& name) {
        if (type == "Light") {
            return new Light(id, name);
        } else if (type == "Climate") {
            return new Climate(id, name);
        } else if (type == "Sensor") {
            return new Sensor(id, name);
        } else if (type == "Shutter") {
            return new Shutter(id, name);
        } else if (type == "ColorLight") {
            return new ColorLight(id, name);
        } else if (type == "DimmLight") {
            return new DimmLight(id, name);
        } else if (type == "Media") {
            return new Media(id, name);
        }

        ESP_LOGE("IoTDeviceFactory", "Invalid device type");

        return nullptr;
    }
};

class IRepository {
   public:
    virtual void addDevice(IoTDevice* device) = 0;
    virtual void removeDevice(const std::string& id) = 0;
    virtual void updateDevice(IoTDevice* device) = 0;
};

class DeviceStore : public IRepository {
   private:
    std::map<std::string, std::unique_ptr<IoTDevice>> devices;
    PubSub<DeviceStatus>* statusPubSub;

   public:
    DeviceStore(PubSub<DeviceStatus>* statusPubSub = nullptr)
        : statusPubSub(statusPubSub) {}

    void addDevice(IoTDevice* device) override {
        if (device) {
            this->devices[device->getId()] = std::unique_ptr<IoTDevice>(device);
        }
    }

    void describeDevice(std::string id) {
        auto it = this->devices.find(id);
        if (it != this->devices.end()) {
            it->second->describe();
        } else {
            ESP_LOGW("DeviceStore", "Device not Found");
        }
    }

    void initFromSettings(std::vector<device_t> devices) {
        for (auto const& dev : devices) {
            this->addDevice(IoTDeviceFactory::create(dev.type, dev.id, dev.name));
        }
    }

    void removeDevice(const std::string& id) override {
        auto it = this->devices.find(id);
        if (it != this->devices.end()) {
            this->devices.erase(it);
        }
    }

    virtual void updateDevice(IoTDevice* device) override {
        if (device) {
            auto it = this->devices.find(device->getId());
            if (it != this->devices.end()) {
                // Implementieren wenn genutzt
                /*   if (it->second.get() != device)
                  {
                      it->second = std::unique_ptr<IoTDevice>(device);
                  } */
            } else {
                this->devices[device->getId()] = std::unique_ptr<IoTDevice>(device);
            }

            this->notifyObservers(*device);
        }
    }

    void notifyObservers(const IoTDevice& device) {
        if (this->statusPubSub != nullptr) {
            DeviceStatus status;
            strcpy(status.device_id, device.getId().c_str());
            this->statusPubSub->publish(status);
        }
    }

    uint32_t getChecksum(const std::vector<std::string>& device_ids) {
        CRC32 crc;
        crc.reset();
        for (auto& dev : device_ids) {
            uint32_t chk = this->getDevice(dev)->getChecksum();
            crc.update(chk);
        }

        return crc.finalize();
    }

    std::size_t getDeviceCount() {
        return this->devices.size();
    }

    std::vector<std::string> getDevices() {
        std::vector<std::string> keys;
        keys.reserve(this->getDeviceCount());
        for (const auto& pair : this->devices) {
            keys.push_back(pair.first);
        }

        return keys;
    }

    IoTDevice* getDevice(std::string id) {
        auto it = this->devices.find(id);
        if (it != this->devices.end()) {
            return it->second.get();
        }

        return nullptr;
    }

    bool setState(std::string id, bool state) {
        return setState(this->getDevice(id), state);
    }

    bool setState(IoTDevice* dev, bool state) {
        if (dev == nullptr) {
            return false;
        }

        dev->setState(state);
        this->notifyObservers(*dev);

        return true;
    }
};