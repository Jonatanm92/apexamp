#pragma once

#include <string>
#include <vector>
#include <functional>

namespace namapp {
namespace cloud {

struct Preset {
    std::string id;
    std::string name;
    std::string author;
    std::string genre;
    std::string downloadUrl;
};

class CloudSyncClient {
public:
    CloudSyncClient();
    ~CloudSyncClient();

    void login(const std::string& username, const std::string& password, std::function<void(bool)> onResult);
    void searchCommunityPresets(const std::string& query, std::function<void(std::vector<Preset>)> onResult);
    void downloadPreset(const std::string& presetId, const std::string& targetPath, std::function<void(bool)> onResult);

private:
    std::string mAuthToken;
    const std::string mApiBaseUrl{"https://api.namapp.cloud/v1"};
    
    // In a real app, we'd use libcurl or juce::URL here
};

} // namespace cloud
} // namespace namapp
