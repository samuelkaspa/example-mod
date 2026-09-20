#include <Geode/Geode.hpp>
#include <Geode/modify/TouchDispatcher.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

static Touch* g_activeTouch = nullptr;
static double g_touchStartTime = 0.0;

class TouchHoldIndicator : public CCNode {
public:
    CCDrawNode* m_circleNode = nullptr;
    CCLabelBMFont* m_timerLabel = nullptr;

    CREATE_FUNC(TouchHoldIndicator);

    bool init() override {
        if (!CCNode::init()) return false;

        m_circleNode = CCDrawNode::create();
        m_circleNode->drawDot(ccp(0, 0), 28.0f, ccc4f(1.0f, 1.0f, 1.0f, 0.4f));
        this->addChild(m_circleNode);

        m_timerLabel = CCLabelBMFont::create("0.0", "bigFont.fnt");
        m_timerLabel->setScale(0.45f);
        m_timerLabel->setPosition(ccp(0, 0));
        this->addChild(m_timerLabel);

        this->scheduleUpdate();
        return true;
    }

    void update(float dt) override {
        if (g_touchStartTime > 0.0) {
            double elapsed = (m_pScheduler->getTime() - g_touchStartTime);
            m_timerLabel->setString(fmt::format("{:.1f}", elapsed).c_str());
        }
    }
};

static TouchHoldIndicator* g_currentIndicator = nullptr;

void spawnExplosionEffect(CCPoint pos, CCNode* parent) {
    if (!parent) return;

    auto explosionNode = CCDrawNode::create();
    explosionNode->setPosition(pos);
    explosionNode->drawDot(ccp(0, 0), 35.0f, ccc4f(1.0f, 1.0f, 1.0f, 0.9f));
    parent->addChild(explosionNode, 999);

    auto scaleTo = CCScaleTo::create(0.5f, 2.2f);
    auto fadeOut = CCFadeOut::create(0.5f);
    auto spawnAnim = CCSpawn::create(scaleTo, fadeOut, nullptr);
    auto removeAction = CCRemoveSelf::create();

    explosionNode->runAction(CCSequence::create(spawnAnim, removeAction, nullptr));
}

class $modify(MyPlayLayer, PlayLayer) {
    void onExit() {
        if (g_currentIndicator) {
            g_currentIndicator->removeFromParent();
            g_currentIndicator = nullptr;
        }
        g_activeTouch = nullptr;
        PlayLayer::onExit();
    }
};

class $modify(CCTouchDispatcher) {
    bool touches(CCSet* touches, CCEvent* event, unsigned int index) {
        auto playLayer = PlayLayer::get();
        if (playLayer && touches && touches->count() > 0) {
            auto touch = static_cast<CCTouch*>(touches->anyObject());
            CCPoint location = touch->getLocation();

            if (index == CCTOUCHBEGAN) {
                g_activeTouch = touch;
                g_touchStartTime = CCDirector::sharedDirector()->getScheduler()->getTime();

                if (g_currentIndicator) g_currentIndicator->removeFromParent();
                g_currentIndicator = TouchHoldIndicator::create();
                g_currentIndicator->setPosition(location);
                playLayer->addChild(g_currentIndicator, 1000);

                spawnExplosionEffect(location, playLayer);

            } else if (index == CCTOUCHMOVED && g_activeTouch == touch) {
                if (g_currentIndicator) g_currentIndicator->setPosition(location);

            } else if ((index == CCTOUCHENDED || index == CCTOUCHCANCELLED) && g_activeTouch == touch) {
                if (g_currentIndicator) {
                    g_currentIndicator->removeFromParent();
                    g_currentIndicator = nullptr;
                }
                spawnExplosionEffect(location, playLayer);
                g_activeTouch = nullptr;
                g_touchStartTime = 0.0;
            }
        }
        return CCTouchDispatcher::touches(touches, event, index);
    }
};
