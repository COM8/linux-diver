#include "BTCoffeeMaker.hpp"
#include "Obfuscator.hpp"
#include "Utils.hpp"
#include "gattlib.h"
#include "logger/Logger.hpp"
#include <bluetooth/sdp.h>
#include <spdlog/spdlog.h>

//---------------------------------------------------------------------------
namespace jutta_bt_driver {
//---------------------------------------------------------------------------
RelevantUUIDs::RelevantUUIDs() noexcept {
    try {
        to_uuid("5a401523-ab2e-2548-c435-08c300000710", &DEFAULT_SERVICE_UUID);
        to_uuid("5A401531-AB2E-2548-C435-08C300000710", &ABOUT_MACHINE_CHARACTERISTIC_UUID);
        to_uuid("5a401524-ab2e-2548-c435-08c300000710", &MACHINE_STATUS_CHARACTERISTIC_UUID);
        to_uuid("5a401530-ab2e-2548-c435-08c300000710", &BARISTA_MODE_CHARACTERISTIC_UUID);
        to_uuid("5a401527-ab2e-2548-c435-08c300000710", &PRODUCT_PROGRESS_CHARACTERISTIC_UUID);
        to_uuid("5a401529-ab2e-2548-c435-08c300000710", &P_MODE_CHARACTERISTIC_UUID);
        to_uuid("5a401538-ab2e-2548-c435-08c300000710", &P_MODE_READ_CHARACTERISTIC_UUID);
        to_uuid("5a401525-ab2e-2548-c435-08c300000710", &START_PRODUCT_CHARACTERISTIC_UUID);
        to_uuid("5A401533-ab2e-2548-c435-08c300000710", &STATISTICS_COMMAND_CHARACTERISTIC_UUID);
        to_uuid("5A401534-ab2e-2548-c435-08c300000710", &STATISTICS_DATA_CHARACTERISTIC_UUID);
        to_uuid("5a401528-ab2e-2548-c435-08c300000710", &UPDATE_PRODUCT_CHARACTERISTIC_UUID);

        to_uuid("5a401623-ab2e-2548-c435-08c300000710", &UART_SERVICE_UUID);
        to_uuid("5a401624-ab2e-2548-c435-08c300000710", &UART_TX_CHARACTERISTIC_UUID);
        to_uuid("5a401625-ab2e-2548-c435-08c300000710", &UART_RX_CHARACTERISTIC_UUID);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("Loading UUIDS failed with: {}", e.what());
        std::terminate();
    }
}

void RelevantUUIDs::to_uuid(const std::string& s, uuid_t* uuid) {
    gattlib_string_to_uuid(s.data(), s.size(), uuid);
}

const RelevantUUIDs BTCoffeeMaker::RELEVANT_UUIDS{};

BTCoffeeMaker::BTCoffeeMaker(std::string&& name, std::string&& addr) : bleDevice(
                                                                           std::move(name),
                                                                           std::move(addr),
                                                                           [this](const std::vector<uint8_t>& data, const uuid_t& uuid) { this->on_characteristic_read(data, uuid); },
                                                                           [this]() { this->on_connected(); },
                                                                           [this]() { this->on_disconnected(); },
                                                                           [this](const std::vector<uint8_t>& data, const uuid_t& uuid) { this->on_characteristic_read(data, uuid); }) {}

std::string BTCoffeeMaker::parse_version(const std::vector<uint8_t>& data, size_t from, size_t to) {
    std::string result;
    for (size_t i = from; i <= to; i++) {
        if (data[i]) {
            result += static_cast<char>(data[i]);
        }
    }
    return result;
}

void BTCoffeeMaker::parse_about_data(const std::vector<uint8_t>& data) {
    blueFrogVersion = parse_version(data, 27, 34);
    coffeeMachineVersion = parse_version(data, 35, 50);
    SPDLOG_DEBUG("Found about data. BlueFrog Version: {} Coffee Machine Version: {}", blueFrogVersion, coffeeMachineVersion);
}

void BTCoffeeMaker::parse_machine_status(const std::vector<uint8_t>& data, uint8_t key) {
    std::vector<std::uint8_t> alertVec = deobfuscate(data, key);
    // If this one fails, the deobfuscation failed:
    assert(alertVec[0] == key);
    // TODO parse bits
    // Start with an offset of one byte, since the first byte should represent the key for deobfuscating.
    SPDLOG_INFO("Machine status alertVec: {}", to_hex_string(alertVec));
}

void BTCoffeeMaker::parse_product_progress(const std::vector<uint8_t>& data, uint8_t key) {
    std::vector<std::uint8_t> actData = deobfuscate(data, key);
}

void BTCoffeeMaker::analyze_man_data() {
    parse_man_data(bleDevice.get_mam_data());
}

void BTCoffeeMaker::parse_man_data(const std::vector<uint8_t>& data) {
    key = data[0];
    bfMajVer = data[1];
    bfMinVer = data[2];
    articleNumber = to_uint16_t_little_endian(data, 4);
    machineNumber = to_uint16_t_little_endian(data, 6);
    serialNumber = to_uint16_t_little_endian(data, 8);
    machineProdDate = to_ymd(data, 10);
    machineProdDateUCHI = to_ymd(data, 12);
    unusedSecond = data[14];
    statusBits = data[15];
}

uint16_t BTCoffeeMaker::to_uint16_t_little_endian(const std::vector<uint8_t>& data, size_t offset) {
    return (static_cast<uint16_t>(data[offset + 1]) << 8) | static_cast<uint16_t>(data[offset]);
}

std::chrono::year_month_day BTCoffeeMaker::to_ymd(const std::vector<uint8_t>& data, size_t offset) {
    uint16_t date = to_uint16_t_little_endian(data, offset);
    return std::chrono::year(((date & 65024) >> 9) + 1990) / ((date & 480) >> 5) / (date & 31);
}

void BTCoffeeMaker::on_characteristic_read(const std::vector<uint8_t>& data, const uuid_t& uuid) {
    std::array<char, MAX_LEN_UUID_STR + 1> uuidStr{};
    gattlib_uuid_to_string(&uuid, uuidStr.data(), uuidStr.size());
    SPDLOG_INFO("Received {} bytes of data from characteristic '{}'.", data.size(), uuidStr.data());

    // About UUID:
    if (gattlib_uuid_cmp(&uuid, &RELEVANT_UUIDS.ABOUT_MACHINE_CHARACTERISTIC_UUID) == GATTLIB_SUCCESS) {
        parse_about_data(data);
    }
    // Machine status:
    else if (gattlib_uuid_cmp(&uuid, &RELEVANT_UUIDS.MACHINE_STATUS_CHARACTERISTIC_UUID) == GATTLIB_SUCCESS) {
        parse_machine_status(data, key);
    }  // Product progress:
    else if (gattlib_uuid_cmp(&uuid, &RELEVANT_UUIDS.PRODUCT_PROGRESS_CHARACTERISTIC_UUID) == GATTLIB_SUCCESS) {
        parse_product_progress(data, key);
    } else {
        // TODO print
    }
}
void BTCoffeeMaker::request_status() {
    bleDevice.read_characteristic(RELEVANT_UUIDS.MACHINE_STATUS_CHARACTERISTIC_UUID);
}

void BTCoffeeMaker::request_progress() {
    bleDevice.read_characteristic(RELEVANT_UUIDS.PRODUCT_PROGRESS_CHARACTERISTIC_UUID);
}

void BTCoffeeMaker::request_about_info() {
    bleDevice.read_characteristic(RELEVANT_UUIDS.ABOUT_MACHINE_CHARACTERISTIC_UUID);
}

bool BTCoffeeMaker::write(const uuid_t& characteristic, const std::vector<uint8_t>& data, bool obfuscate) {
    std::vector<uint8_t> obfuscatedData = data;
    if (obfuscate) {
        obfuscatedData[0] = key;
        obfuscatedData = deobfuscate(obfuscatedData, key);
    }
    return bleDevice.write(characteristic, obfuscatedData);
}

void BTCoffeeMaker::restart_coffee_maker() {
    static const std::vector<uint8_t> command{0x00, 0x46, 0x02};
    write(RELEVANT_UUIDS.P_MODE_CHARACTERISTIC_UUID, command, true);
}

void BTCoffeeMaker::on_connected() {
    // Ensure we have the key for deobfuscation ready:
    analyze_man_data();

    // Read the initial status:
    // bleDevice.read_characteristics();
    request_status();
    request_progress();
    request_about_info();
}

void BTCoffeeMaker::on_disconnected() {
}

bool BTCoffeeMaker::connect() {
    return bleDevice.connect();
}

bool BTCoffeeMaker::is_connected() {
    return bleDevice.is_connected();
}
//---------------------------------------------------------------------------
}  // namespace jutta_bt_driver
//---------------------------------------------------------------------------