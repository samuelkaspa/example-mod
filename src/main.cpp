#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <vector>
#include <string>
#include <chrono>

using namespace geode::prelude;

// ==========================================
// ESTRUTURAS E CONFIGURAÇÕES GLOBAIS
// ==========================================
struct ClickPoint {
    cocos2d::CCPoint position;
    float holdTime;
    cocos2d::ccColor3B color;
    bool isHold;
};

struct SavedRun {
    std::string name;
    std::vector<ClickPoint> points;
};

class ModSettings {
public:
    static inline bool touchMobileEnabled = true;
    static inline bool clickTrackerEnabled = true;
    static inline bool trackerHackEnabled = true;
    static inline bool safeModeEnabled = false;
    static inline bool lockedPoints = false;

    // Configurações Visuais
    static inline float textOpacity = 1.0f;
    static inline float checkpointOpacity = 0.8f;
    static inline cocos2d::ccColor3B dotColor = {0, 255, 0};
    static inline cocos2d::ccColor3B explosionColor = {255, 255, 255};
    static inline float dotOpacity = 1.0f;
    static inline float precisionThreshold = 1.0f; // Precisão de proximidade (padrão 1)

    static inline std::vector<ClickPoint> mode2Points;
    static inline std::vector<ClickPoint> mode3Points;
    static inline std::vector<SavedRun> savedRuns;
};

// ==========================================
// 1. TOUCH MOBILE INDICATOR (Efeito Visual)
// ==========================================
class TouchMobileNode : public cocos2d::CCNode {
public:
    cocos2d::CCDrawNode* drawNode = nullptr;
    cocos2d::CCLabelBMFont* timerLabel = nullptr;
    cocos2d::CCMotionStreak* streak = nullptr;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    bool isPressed = false;

    static TouchMobileNode* create() {
        auto ret = new TouchMobileNode();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    bool init() override {
        drawNode = cocos2d::CCDrawNode::create();
        this->addChild(drawNode);

        // Cronômetro no centro (não gira)
        timerLabel = cocos2d::CCLabelBMFont::create("0.000s", "chatFont.fnt");
        timerLabel->setScale(0.5f);
        timerLabel->setOpacity(static_cast<GLubyte>(ModSettings::textOpacity * 255));
        this->addChild(timerLabel, 10);

        // Rastro (Trail) quando arrastado
        streak = cocos2d::CCMotionStreak::create(0.3f, 1.0f, 10.0f, cocos2d::ccc3(255, 255, 255), "square.png");
        this->addChild(streak);

        this->scheduleUpdate();
        return true;
    }

    void startTouch(cocos2d::CCPoint pos) {
        this->setPosition(pos);
        isPressed = true;
        startTime = std::chrono::high_resolution_clock::now();
        this->setVisible(true);
        this->setOpacity(255);
    }

    void updateTouch(cocos2d::CCPoint pos) {
        this->setPosition(pos);
        if (streak) streak->setPosition(pos);
    }

    void releaseTouch() {
        isPressed = false;
        // Efeito de explosão e sumiço em 0.5s
        drawNode->runAction(cocos2d::CCScaleTo::create(0.5f, 2.0f));
        this->runAction(cocos2d::CCSequence::create(
            cocos2d::CCFadeOut::create(0.5f),
            cocos2d::CCCallFunc::create(this, callfunc_selector(TouchMobileNode::resetNode)),
            nullptr
        ));
    }

    void resetNode() {
        this->setVisible(false);
        drawNode->setScale(1.0f);
    }

    void update(float dt) override {
        if (isPressed) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> elapsed = now - startTime;
            
            // Atualiza tempo pressionado sem girar o texto
            char buffer[16];
            snprintf(buffer, sizeof(buffer), "%.3fs", elapsed.count());
            timerLabel->setString(buffer);

            // Animação da bolinha detalhada girando
            drawNode->clear();
            drawNode->setRotation(drawNode->getRotation() + 180.0f * dt);
            drawNode->drawCircle(cocos2d::CCPoint(0, 0), 15.0f, ModSettings::dotColor, 1.5f, 16);
        }
    }
};

// ==========================================
// POPUP DE CONFIGURAÇÕES (In-Game Menu)
// ==========================================
class ModSettingsPopup : public geode::Popup<> {
protected:
    bool setup() override {
        this->setTitle("Configurações do Mod");

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        // Botão Safe Mode
        auto safeToggle = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(ModSettingsPopup::onToggleSafeMode), 0.7f
        );
        safeToggle->toggle(ModSettings::safeModeEnabled);
        
        auto menu = CCMenu::create();
        menu->addChild(safeToggle);
        menu->setPosition({winSize.width / 2, winSize.height / 2});
        this->m_mainLayer->addChild(menu);

        return true;
    }

    void onToggleSafeMode(CCObject*) {
        ModSettings::safeModeEnabled = !ModSettings::safeModeEnabled;
    }

public:
    static ModSettingsPopup* create() {
        auto ret = new ModSettingsPopup();
        if (ret && ret->initAnchored(320.0f, 240.0f)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

// ==========================================
// 2 & 3. HOOKS NO PLAYLAYER (Gameplay)
// ==========================================
class $modify(MyPlayLayer, PlayLayer) {
    TouchMobileNode* touchNode = nullptr;

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        // Criar nó de toque mobile
        m_fields->touchNode = TouchMobileNode::create();
        m_fields->touchNode->setVisible(false);
        this->addChild(m_fields->touchNode, 999);

        return true;
    }

    void pushButton(int player, bool isPush) {
        PlayLayer::pushButton(player, isPush);

        // Se estiver no Practice Mode, desativa o Tracker Hack
        if (this->m_isPracticeMode) return;

        cocos2d::CCPoint playerPos = this->m_player1->getPosition();

        // Modo 2: Click Tracker
        if (ModSettings::clickTrackerEnabled) {
            ModSettings::mode2Points.push_back({playerPos, 0.0f, {0, 255, 0}, false});
        }

        // Modo 3: Tracker Hack
        if (ModSettings::trackerHackEnabled && !ModSettings::lockedPoints) {
            ModSettings::mode3Points.push_back({playerPos, 0.0f, ModSettings::dotColor, false});
        }
    }

    void resetLevel() override {
        PlayLayer::resetLevel();

        // Modo 2: Apaga as bolinhas ao morrer
        if (ModSettings::clickTrackerEnabled) {
            ModSettings::mode2Points.clear();
        }

        // Modo 3: Não apaga as bolinhas no respawn!
    }

    void levelComplete() override {
        if (ModSettings::safeModeEnabled) {
            // Safe Mode ativo: Não conclui o nível oficialmente
            ModSettings::lockedPoints = true;
            FLAlertLayer::create("Safe Mode =D", "Nível concluído em modo seguro. O progresso não foi salvo.", "OK")->show();
            return;
        }

        ModSettings::lockedPoints = true; // Trava após concluir o nível
        PlayLayer::levelComplete();
    }
};

// ==========================================
// HOOK NO PAUSELAYER (Botão de Pausa)
// ==========================================
class $modify(MyPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();

        // Criar botão no lado direito (meio-termo, sem encostar na borda)
        auto btnSprite = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
        btnSprite->setScale(0.6f);

        auto btn = CCMenuItemSpriteExtra::create(
            btnSprite, this, menu_selector(MyPauseLayer::onOpenModSettings)
        );

        auto menu = CCMenu::create();
        menu->setPosition({winSize.width - 60.0f, winSize.height / 2.0f});
        menu->addChild(btn);
        this->addChild(menu, 10);
    }

    void onOpenModSettings(CCObject*) {
        ModSettingsPopup::create()->show();
    }
};
