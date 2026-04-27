#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/GameObject.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

// ------------------------------------------------------------
//  Lớp AutoBot: chứa logic quét và nhảy
// ------------------------------------------------------------
class AutoBot {
public:
    // Singleton
    static AutoBot* get() {
        static AutoBot instance;
        return &instance;
    }

    // Đọc trạng thái BẬT/TẮT từ cài đặt Geode
    bool isEnabled() {
        return Mod::get()->getSettingValue<bool>("enabled");
    }

    // ---------------------------------------------------------
    //  update(): được gọi mỗi khung hình khi bot được bật
    //  Quét m_objects tìm chướng ngại vật gần nhất,
    //  tính khoảng cách và gửi lệnh nhảy nếu cần.
    // ---------------------------------------------------------
    void update(PlayLayer* pl, float dt) {
        // 1. KIỂM TRA ĐIỀU KIỆN TẮT -> không xử lý gì cả
        if (!isEnabled()) return;

        // 2. Không có người chơi hoặc đã chết / tạm dừng -> thoát
        if (!pl->m_player1) return;
        if (pl->m_player1->m_isDead) return;
        if (pl->m_isPaused) return;

        // 3. Nếu đang trong thời gian “hồi chiêu” giữa các lần nhảy thì bỏ qua
        m_cooldown -= dt;
        if (m_cooldown > 0.0f) return;

        PlayerObject* player = pl->m_player1;
        float playerX = player->getPositionX();
        float playerY = player->getPositionY();

        // Lấy hệ số tốc độ của level để điều chỉnh ngưỡng phát hiện
        float speedFactor = 1.0f;
        if (pl->m_levelSettings) {
            speedFactor = pl->m_levelSettings->m_speed;
        }
        // Ngưỡng khoảng cách phía trước để kích hoạt nhảy (càng tốc độ cao càng cần nhìn xa)
        float threshold = 120.0f * speedFactor;

        float closestDist = threshold + 1.0f;
        GameObject* closestObj = nullptr;

        // 4. DUYỆT TOÀN BỘ ĐỐI TƯỢNG TRONG m_objects
        CCARRAY_FOREACH_BASE(pl->m_objects, obj, GameObject*, ix) {
            // Chỉ quan tâm đến gai (type 2) và cưa (type 8)
            int objType = obj->m_objectType;
            if (objType != 2 && objType != 8) continue;
            // Bỏ qua nếu là trigger (không phải vật cản thực sự)
            if (obj->m_isTrigger) continue;
            // Phải đang hiển thị trên màn hình
            if (!obj->isVisible()) continue;

            float objX = obj->getPositionX();
            float objY = obj->getPositionY();

            // Chỉ xét vật cản phía trước người chơi
            if (objX <= playerX) continue;

            float dist = objX - playerX;
            if (dist > threshold) continue;

            // Kiểm tra chiều cao: phải nằm trong cùng hàng với player (±30 đơn vị)
            if (std::abs(objY - playerY) > 30.0f) continue;

            // Tìm vật cản gần nhất
            if (dist < closestDist) {
                closestDist = dist;
                closestObj = obj;
            }
        }

        // 5. NẾU TÌM THẤY VẬT CẢN GẦN NHẤT VÀ CHƯA KÍCH HOẠT…
        if (closestObj) {
            if (m_lastTriggered != closestObj) {
                // Gửi lệnh nhảy
                pl->pushButton(PlayerButton::Jump, false);
                pl->releaseButton(PlayerButton::Jump, false); // tap nhanh
                m_lastTriggered = closestObj;
                m_cooldown = m_cooldownTime; // đặt thời gian chờ 0.2s
            }
        } else {
            // Không có vật cản nào trong tầm -> reset bộ nhớ vật cản trước đó
            m_lastTriggered = nullptr;
        }
    }

private:
    GameObject* m_lastTriggered = nullptr;   // vật cản đã được nhảy qua lần trước
    float m_cooldown = 0.0f;                 // bộ đếm thời gian chờ giữa các nhảy
    const float m_cooldownTime = 0.2f;       // thời gian chờ = 200ms (đủ cho một lần tap)
};

// ------------------------------------------------------------
//  Hook vào PlayLayer bằng macro $modify của Geode
//  Tự động gọi AutoBot::update() sau khi game update xong.
// ------------------------------------------------------------
class $modify(PlayLayer) {
    // Lưu con trỏ đến AutoBot (tối ưu, tránh gọi get() mỗi frame)
    struct Fields {
        AutoBot* bot;
    };

    // Khi PlayLayer khởi tạo, liên kết với AutoBot singleton
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects))
            return false;
        m_fields->bot = AutoBot::get();
        return true;
    }

    // Override hàm update: gọi code gốc, sau đó chạy bot
    void update(float dt) {
        PlayLayer::update(dt);  // giữ nguyên logic gốc
        if (m_fields->bot) {
            m_fields->bot->update(this, dt);
        }
    }
};
