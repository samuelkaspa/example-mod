#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace geode::prelude;

// ---------------------------------------------------------------
// Lua
// ---------------------------------------------------------------
static lua_State* g_L = nullptr;

static int l_print(lua_State* L) {
    int n = lua_gettop(L);
    std::string out;
    for (int i = 1; i <= n; i++) {
        size_t len;
        const char* s = luaL_tolstring(L, i, &len);
        out.append(s, len);
        if (i < n) out += " ";
        lua_pop(L, 1);
    }
    log::info("[Lua] {}", out);
    Notification::create(out, NotificationIcon::None, 2.f)->show();
    return 0;
}

// speed(2.0) -> muda a velocidade do jogo
static int l_speed(lua_State* L) {
    float s = (float)luaL_checknumber(L, 1);
    CCDirector::get()->getScheduler()->setTimeScale(s);
    return 0;
}

// x, y = player_pos()
static int l_player_pos(lua_State* L) {
    auto pl = PlayLayer::get();
    if (!pl) { lua_pushnil(L); return 1; }
    auto p = pl->m_player1->getPosition();
    lua_pushnumber(L, p.x);
    lua_pushnumber(L, p.y);
    return 2;
}

// set_player_pos(x, y)
static int l_set_player_pos(lua_State* L) {
    auto pl = PlayLayer::get();
    if (!pl) return 0;
    pl->m_player1->setPosition({
        (float)luaL_checknumber(L, 1),
        (float)luaL_checknumber(L, 2)
    });
    return 0;
}

static void initLua() {
    if (g_L) return;
    g_L = luaL_newstate();
    luaL_openlibs(g_L);
    lua_register(g_L, "print", l_print);
    lua_register(g_L, "speed", l_speed);
    lua_register(g_L, "player_pos", l_player_pos);
    lua_register(g_L, "set_player_pos", l_set_player_pos);
}

// retorna "" se deu certo, ou a mensagem de erro
static std::string runLua(std::string const& code) {
    initLua();
    if (luaL_dostring(g_L, code.c_str()) != LUA_OK) {
        std::string err = lua_tostring(g_L, -1) ? lua_tostring(g_L, -1) : "erro desconhecido";
        lua_pop(g_L, 1);
        return err;
    }
    return "";
}

// ---------------------------------------------------------------
// Popup com caixa de texto + botao Confirmar
// ---------------------------------------------------------------
class CodePopup : public Popup<> {
protected:
    TextInput* m_input = nullptr;

    bool setup() override {
        this->setTitle("Lua Console");

        m_input = TextInput::create(260.f, "Cole seu codigo Lua aqui", "chatFont.fnt");
        m_input->setMaxCharCount(20000);
        m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 10.f});

        auto spr = ButtonSprite::create("Confirmar");
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this](CCObject*) {
            auto err = runLua(m_input->getString());
            if (err.empty()) {
                Notification::create("Codigo executado!", NotificationIcon::Success)->show();
                this->onClose(nullptr);
            } else {
                FLAlertLayer::create("Erro no codigo", err, "OK")->show();
            }
        });
        m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0.f, 25.f});
        return true;
    }

public:
    static CodePopup* create() {
        auto ret = new CodePopup();
        if (ret->initAnchored(300.f, 160.f)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// ---------------------------------------------------------------
// Bolinha arrastavel (fica em todas as telas)
// ---------------------------------------------------------------
class FloatingBall : public CCLayer {
protected:
    CCSprite* m_ball = nullptr;
    CCPoint m_start, m_offset;
    bool m_moved = false;

    bool init() override {
        if (!CCLayer::init()) return false;

        auto win = CCDirector::get()->getWinSize();

        m_ball = CCSprite::createWithSpriteFrameName("GJ_button_01.png");
        m_ball->setScale(0.8f);
        m_ball->setOpacity(200);
        m_ball->setPosition({35.f, win.height / 2});
        this->addChild(m_ball);

        auto lbl = CCLabelBMFont::create("</>", "bigFont.fnt");
        lbl->setScale(0.45f);
        lbl->setPosition(m_ball->getContentSize() / 2);
        m_ball->addChild(lbl);

        this->setTouchEnabled(true);
        return true;
    }

    void registerWithTouchDispatcher() override {
        CCTouchDispatcher::get()->addTargetedDelegate(this, -600, true);
    }

    bool ccTouchBegan(CCTouch* t, CCEvent*) override {
        auto p = this->convertToNodeSpace(t->getLocation());
        float r = m_ball->getContentSize().width * m_ball->getScale() / 2;
        if (ccpDistance(p, m_ball->getPosition()) > r) return false;
        m_start = p;
        m_offset = m_ball->getPosition() - p;
        m_moved = false;
        return true; // so engole o toque se acertou a bolinha
    }

    void ccTouchMoved(CCTouch* t, CCEvent*) override {
        auto p = this->convertToNodeSpace(t->getLocation());
        if (!m_moved && ccpDistance(p, m_start) > 8.f) m_moved = true;
        if (m_moved) {
            auto win = CCDirector::get()->getWinSize();
            auto np = p + m_offset;
            np.x = std::clamp(np.x, 15.f, win.width - 15.f);
            np.y = std::clamp(np.y, 15.f, win.height - 15.f);
            m_ball->setPosition(np);
        }
    }

    void ccTouchEnded(CCTouch*, CCEvent*) override {
        if (m_moved) return;
        auto scene = CCDirector::get()->getRunningScene();
        if (scene && scene->getChildByType<CodePopup>(0)) return; // ja aberto
        if (auto popup = CodePopup::create()) popup->show();
    }

    void ccTouchCancelled(CCTouch*, CCEvent*) override {}

public:
    CREATE_FUNC(FloatingBall);
};

// ---------------------------------------------------------------
// Cria a bolinha uma vez, quando o menu principal abre
// ---------------------------------------------------------------
class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool added = false;
        if (!added) {
            added = true;
            auto ball = FloatingBall::create();
            ball->setZOrder(10000);
            SceneManager::get()->keepAcrossScenes(ball);
        }
        return true;
    }
};
