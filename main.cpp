#include <SFML/Graphics.hpp>
#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>

// ─── Constants ───────────────────────────────────────────────────────────────
const float WINDOW_W     = 1000.f;
const float WINDOW_H     = 700.f;
const float FIELD_LEFT   = 80.f;
const float FIELD_TOP    = 80.f;
const float FIELD_RIGHT  = 920.f;
const float FIELD_BOTTOM = 620.f;
const float GOAL_W       = 18.f;
const float GOAL_H       = 120.f;
const float GOAL_TOP     = (WINDOW_H - GOAL_H) / 2.f;
const float PLAYER_R     = 18.f;
const float BALL_R       = 11.f;
const float PLAYER_SPEED = 220.f;
const float AI_SPEED     = 190.f;
const float BALL_FRICTION= 0.985f;
const float KICK_FORCE   = 380.f;

// ─── Helpers ─────────────────────────────────────────────────────────────────
float length(sf::Vector2f v) {
    return std::sqrt(v.x * v.x + v.y * v.y);
}
sf::Vector2f normalize(sf::Vector2f v) {
    float l = length(v);
    if (l == 0.f) return {0.f, 0.f};
    return {v.x / l, v.y / l};
}

// ─── Player ──────────────────────────────────────────────────────────────────
struct Player {
    sf::CircleShape shape;
    sf::Vector2f    velocity;
    bool            isAI;

    Player(float x, float y, sf::Color color, bool ai = false) : isAI(ai) {
        shape.setRadius(PLAYER_R);
        shape.setOrigin({PLAYER_R, PLAYER_R});
        shape.setPosition({x, y});
        shape.setFillColor(color);
        shape.setOutlineThickness(2.f);
        shape.setOutlineColor(sf::Color::White);
        velocity = {0.f, 0.f};
    }

    sf::Vector2f getPos() const { return shape.getPosition(); }

    void update(float dt) {
        shape.move(velocity * dt);
        sf::Vector2f p = getPos();
        if (p.x - PLAYER_R < FIELD_LEFT)   { shape.setPosition({FIELD_LEFT  + PLAYER_R, p.y}); velocity.x = 0; }
        if (p.x + PLAYER_R > FIELD_RIGHT)  { shape.setPosition({FIELD_RIGHT - PLAYER_R, p.y}); velocity.x = 0; }
        if (p.y - PLAYER_R < FIELD_TOP)    { shape.setPosition({p.x, FIELD_TOP    + PLAYER_R}); velocity.y = 0; }
        if (p.y + PLAYER_R > FIELD_BOTTOM) { shape.setPosition({p.x, FIELD_BOTTOM - PLAYER_R}); velocity.y = 0; }
    }

    void draw(sf::RenderWindow& win) { win.draw(shape); }
};

// ─── Ball ────────────────────────────────────────────────────────────────────
struct Ball {
    sf::CircleShape shape;
    sf::Vector2f    velocity;

    Ball() {
        shape.setRadius(BALL_R);
        shape.setOrigin({BALL_R, BALL_R});
        shape.setFillColor(sf::Color::White);
        shape.setOutlineThickness(2.f);
        shape.setOutlineColor(sf::Color(180,180,180));
        reset();
    }

    sf::Vector2f getPos() const { return shape.getPosition(); }

    void reset() {
        shape.setPosition({WINDOW_W / 2.f, WINDOW_H / 2.f});
        velocity = {0.f, 0.f};
    }

    void update(float dt) {
        velocity *= BALL_FRICTION;
        shape.move(velocity * dt);

        sf::Vector2f p = getPos();

        // left wall — only bounce if NOT in goal zone
        if (p.x - BALL_R < FIELD_LEFT) {
            if (p.y < GOAL_TOP || p.y > GOAL_TOP + GOAL_H) {
                // not a goal, bounce
                shape.setPosition({FIELD_LEFT + BALL_R + 1.f, p.y});
                velocity.x = std::abs(velocity.x) * 0.8f;
            }
            // if in goal zone, let it pass through (goal detected in main)
        }

        // right wall — only bounce if NOT in goal zone
        if (p.x + BALL_R > FIELD_RIGHT) {
            if (p.y < GOAL_TOP || p.y > GOAL_TOP + GOAL_H) {
                shape.setPosition({FIELD_RIGHT - BALL_R - 1.f, p.y});
                velocity.x = -std::abs(velocity.x) * 0.8f;
            }
        }

        if (p.y - BALL_R < FIELD_TOP)    { shape.setPosition({p.x, FIELD_TOP    + BALL_R + 1.f}); velocity.y =  std::abs(velocity.y) * 0.8f; }
        if (p.y + BALL_R > FIELD_BOTTOM) { shape.setPosition({p.x, FIELD_BOTTOM - BALL_R - 1.f}); velocity.y = -std::abs(velocity.y) * 0.8f; }

        // corner escape
        p = getPos();
        float spd = length(velocity);
        bool nearLeft   = p.x - BALL_R < FIELD_LEFT   + 5.f;
        bool nearRight  = p.x + BALL_R > FIELD_RIGHT  - 5.f;
        bool nearTop    = p.y - BALL_R < FIELD_TOP    + 5.f;
        bool nearBottom = p.y + BALL_R > FIELD_BOTTOM - 5.f;

        if (spd < 30.f && (nearLeft || nearRight) && (nearTop || nearBottom)) {
            sf::Vector2f center = {WINDOW_W / 2.f, WINDOW_H / 2.f};
            velocity = normalize(center - p) * 120.f;
        }
    }

    void draw(sf::RenderWindow& win) { win.draw(shape); }
};

// ─── Collision player <-> ball ───────────────────────────────────────────────
void handleKick(Player& pl, Ball& ball) {
    sf::Vector2f diff = ball.getPos() - pl.getPos();
    float dist = length(diff);
    if (dist < PLAYER_R + BALL_R) {
        sf::Vector2f dir = normalize(diff);

        if (length(dir) < 0.01f)
            dir = normalize(sf::Vector2f{WINDOW_W / 2.f, WINDOW_H / 2.f} - ball.getPos());

        ball.shape.setPosition(pl.getPos() + dir * (PLAYER_R + BALL_R + 2.f));

        sf::Vector2f kickVel = pl.velocity * 0.5f + dir * KICK_FORCE;
        if (length(kickVel) < 80.f)
            kickVel = dir * 80.f;

        ball.velocity = kickVel;
    }
}

// ─── AI logic ────────────────────────────────────────────────────────────────

// Full AI opponent (enemy team) — role based
void updateAI(Player& pl, const Ball& ball, float targetX, int index, int total, float dt) {
    sf::Vector2f ballPos = ball.getPos();
    sf::Vector2f myPos   = pl.getPos();

    sf::Vector2f target;

    if (total == 1) {
        if (length(ballPos - myPos) < 150.f)
            target = {targetX, ballPos.y};
        else
            target = ballPos;
    } else {
        float roleRatio = (float)index / (float)(total - 1);

        if (roleRatio < 0.35f) {
            if (length(ballPos - myPos) < 150.f)
                target = {targetX, ballPos.y};
            else
                target = ballPos;
        } else if (roleRatio < 0.7f) {
            float offsetY = (index % 2 == 0) ? -90.f : 90.f;
            target = {
                (ballPos.x + targetX) / 2.f,
                ballPos.y + offsetY
            };
        } else {
            float defendX = (targetX > WINDOW_W / 2.f)
                            ? WINDOW_W * 0.72f
                            : WINDOW_W * 0.28f;
            target = { defendX, ballPos.y };
        }
    }

    target.x = std::max(FIELD_LEFT  + PLAYER_R, std::min(FIELD_RIGHT  - PLAYER_R, target.x));
    target.y = std::max(FIELD_TOP   + PLAYER_R, std::min(FIELD_BOTTOM - PLAYER_R, target.y));

    sf::Vector2f dir = normalize(target - myPos);
    float dist = length(target - myPos);
    float speed = (dist < 40.f) ? AI_SPEED * (dist / 40.f) : AI_SPEED;

    pl.velocity = dir * speed;
    pl.update(dt);
}

// Teammate AI — each player gets a role based on their index
void updateTeammateAI(Player& pl, const Ball& ball, float attackX, int index, int total, float dt) {
    sf::Vector2f ballPos = ball.getPos();
    sf::Vector2f myPos   = pl.getPos();

    sf::Vector2f target;

    if (total == 1) {
        target = ballPos;
    } else {
        float roleRatio = (float)index / (float)(total - 1);

        if (roleRatio < 0.35f) {
            target = ballPos;
        } else if (roleRatio < 0.7f) {
            float offsetY = (index % 2 == 0) ? -80.f : 80.f;
            target = {
                (ballPos.x + attackX) / 2.f,
                ballPos.y + offsetY
            };
        } else {
            float defendX = (attackX > WINDOW_W / 2.f)
                            ? WINDOW_W * 0.30f
                            : WINDOW_W * 0.70f;
            target = { defendX, ballPos.y };
        }
    }

    target.x = std::max(FIELD_LEFT  + PLAYER_R, std::min(FIELD_RIGHT  - PLAYER_R, target.x));
    target.y = std::max(FIELD_TOP   + PLAYER_R, std::min(FIELD_BOTTOM - PLAYER_R, target.y));

    sf::Vector2f dir = normalize(target - myPos);
    float dist = length(target - myPos);
    float speed = (dist < 40.f) ? AI_SPEED * (dist / 40.f) : AI_SPEED;

    pl.velocity = dir * speed;
    pl.update(dt);
}

// ─── Draw field ──────────────────────────────────────────────────────────────
void drawField(sf::RenderWindow& win) {
    sf::RectangleShape grass({FIELD_RIGHT - FIELD_LEFT, FIELD_BOTTOM - FIELD_TOP});
    grass.setPosition({FIELD_LEFT, FIELD_TOP});
    grass.setFillColor(sf::Color(34, 139, 34));
    grass.setOutlineThickness(3.f);
    grass.setOutlineColor(sf::Color::White);
    win.draw(grass);

    sf::RectangleShape centerLine({3.f, FIELD_BOTTOM - FIELD_TOP});
    centerLine.setPosition({WINDOW_W / 2.f - 1.5f, FIELD_TOP});
    centerLine.setFillColor(sf::Color(255,255,255,160));
    win.draw(centerLine);

    sf::CircleShape centerCircle(60.f);
    centerCircle.setOrigin({60.f, 60.f});
    centerCircle.setPosition({WINDOW_W / 2.f, WINDOW_H / 2.f});
    centerCircle.setFillColor(sf::Color::Transparent);
    centerCircle.setOutlineThickness(2.f);
    centerCircle.setOutlineColor(sf::Color(255,255,255,160));
    win.draw(centerCircle);

    sf::RectangleShape leftGoal({GOAL_W, GOAL_H});
    leftGoal.setPosition({FIELD_LEFT - GOAL_W, GOAL_TOP});
    leftGoal.setFillColor(sf::Color(255,255,255,60));
    leftGoal.setOutlineThickness(2.f);
    leftGoal.setOutlineColor(sf::Color::White);
    win.draw(leftGoal);

    sf::RectangleShape rightGoal({GOAL_W, GOAL_H});
    rightGoal.setPosition({FIELD_RIGHT, GOAL_TOP});
    rightGoal.setFillColor(sf::Color(255,255,255,60));
    rightGoal.setOutlineThickness(2.f);
    rightGoal.setOutlineColor(sf::Color::White);
    win.draw(rightGoal);
}

// ─── Check goal ──────────────────────────────────────────────────────────────
int checkGoal(const Ball& ball) {
    sf::Vector2f p = ball.getPos();
    // left goal — ball crosses left wall inside goal height
    if (p.x - BALL_R < FIELD_LEFT - 2.f &&
        p.y + BALL_R > GOAL_TOP && p.y - BALL_R < GOAL_TOP + GOAL_H) return 2;
    // right goal — ball crosses right wall inside goal height
    if (p.x + BALL_R > FIELD_RIGHT + 2.f &&
        p.y + BALL_R > GOAL_TOP && p.y - BALL_R < GOAL_TOP + GOAL_H) return 1;
    return 0;
}
// ─── Spawn helpers ───────────────────────────────────────────────────────────
std::vector<Player> spawnTeam(int count, bool leftSide, sf::Color color, bool ai) {
    std::vector<Player> team;
    float x = leftSide ? WINDOW_W * 0.25f : WINDOW_W * 0.75f;
    float spacing = (FIELD_BOTTOM - FIELD_TOP) / (count + 1.f);
    for (int i = 0; i < count; i++) {
        float y = FIELD_TOP + spacing * (i + 1);
        team.emplace_back(x, y, color, ai);
    }
    return team;
}

// ─── Menu screen ─────────────────────────────────────────────────────────────
struct GameConfig {
    int  teamSize = 3;
    bool pvp      = false;
};

GameConfig showMenu(sf::RenderWindow& win, sf::Font& font) {
    GameConfig cfg;
    int teamSize = 3;
    bool pvp = false;

    while (win.isOpen()) {
        while (const std::optional event = win.pollEvent()) {
            if (event->is<sf::Event::Closed>()) win.close();
            if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Up   && teamSize < 5) teamSize++;
                if (key->code == sf::Keyboard::Key::Down && teamSize > 1) teamSize--;
                if (key->code == sf::Keyboard::Key::Tab) pvp = !pvp;
                if (key->code == sf::Keyboard::Key::Enter) {
                    cfg.teamSize = teamSize;
                    cfg.pvp      = pvp;
                    return cfg;
                }
            }
        }

        win.clear(sf::Color(20, 80, 20));

        sf::Text title(font, "TOP-DOWN FOOTBALL", 48);
        title.setFillColor(sf::Color::White);
        title.setPosition({WINDOW_W/2.f - title.getGlobalBounds().size.x/2.f, 120.f});
        win.draw(title);

        std::ostringstream ss;
        ss << "Team Size: " << teamSize << "   (UP/DOWN to change)";
        sf::Text sizeText(font, ss.str(), 28);
        sizeText.setFillColor(sf::Color::Yellow);
        sizeText.setPosition({WINDOW_W/2.f - sizeText.getGlobalBounds().size.x/2.f, 280.f});
        win.draw(sizeText);

        std::string modeStr = std::string("Mode: ") + (pvp ? "Player vs Player" : "Player vs Computer") + "   (TAB to switch)";
        sf::Text modeText(font, modeStr, 28);
        modeText.setFillColor(sf::Color::Cyan);
        modeText.setPosition({WINDOW_W/2.f - modeText.getGlobalBounds().size.x/2.f, 350.f});
        win.draw(modeText);

        sf::Text ctrl(font, "ENTER to Start", 32);
        ctrl.setFillColor(sf::Color::White);
        ctrl.setPosition({WINDOW_W/2.f - ctrl.getGlobalBounds().size.x/2.f, 460.f});
        win.draw(ctrl);

        sf::Text ctrl2(font, "P1: WASD    P2 / AI target: Arrow Keys", 22);
        ctrl2.setFillColor(sf::Color(200,200,200));
        ctrl2.setPosition({WINDOW_W/2.f - ctrl2.getGlobalBounds().size.x/2.f, 530.f});
        win.draw(ctrl2);

        win.display();
    }
    return cfg;
}

// ─── Main ────────────────────────────────────────────────────────────────────
int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({(unsigned)WINDOW_W, (unsigned)WINDOW_H}),
                            "Top-Down Football");
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        return -1;
    }

    while (window.isOpen()) {
        GameConfig cfg = showMenu(window, font);
        if (!window.isOpen()) break;

        std::vector<Player> teamA = spawnTeam(cfg.teamSize, true,  sf::Color(30,100,255), false);
        std::vector<Player> teamB = spawnTeam(cfg.teamSize, false, sf::Color(220,40,40),  !cfg.pvp);

        Ball ball;
        int scoreA = 0, scoreB = 0;
        sf::Clock clock;

        while (window.isOpen()) {
            float dt = clock.restart().asSeconds();
            if (dt > 0.05f) dt = 0.05f;

            while (const std::optional event = window.pollEvent()) {
                if (event->is<sf::Event::Closed>()) window.close();
                if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
                    if (key->code == sf::Keyboard::Key::Escape) goto backToMenu;
                }
            }

            // ── Input Team A (WASD) ──
            {
                sf::Vector2f dir{0,0};
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) dir.y -= 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) dir.y += 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dir.x -= 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dir.x += 1;
                dir = normalize(dir);
                int total = (int)teamA.size();
                for (int i = 0; i < total; i++) {
                    if (i == 0) {
                        teamA[i].velocity = dir * PLAYER_SPEED;
                        teamA[i].update(dt);
                    } else {
                        updateTeammateAI(teamA[i], ball, FIELD_RIGHT + 10.f, i - 1, total - 1, dt);
                    }
                }
            }

            // ── Input Team B ──
            if (cfg.pvp) {
                sf::Vector2f dir{0,0};
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up))    dir.y -= 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down))  dir.y += 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left))  dir.x -= 1;
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right)) dir.x += 1;
                dir = normalize(dir);
                int total = (int)teamB.size();
                for (int i = 0; i < total; i++) {
                    if (i == 0) {
                        teamB[i].velocity = dir * PLAYER_SPEED;
                        teamB[i].update(dt);
                    } else {
                        updateTeammateAI(teamB[i], ball, FIELD_LEFT - 10.f, i - 1, total - 1, dt);
                    }
                }
            } else {
                int total = (int)teamB.size();
                for (int i = 0; i < total; i++)
                    updateAI(teamB[i], ball, FIELD_LEFT - 10.f, i, total, dt);
            }

            // ── Ball ──
            ball.update(dt);

            // ── Kicks ──
            for (auto& p : teamA) handleKick(p, ball);
            for (auto& p : teamB) handleKick(p, ball);

            // ── Goal check ──
            int goal = checkGoal(ball);
            if (goal == 1) { scoreA++; ball.reset(); }
            if (goal == 2) { scoreB++; ball.reset(); }

            // ── Draw ──
            window.clear(sf::Color(10, 60, 10));
            drawField(window);
            for (auto& p : teamA) p.draw(window);
            for (auto& p : teamB) p.draw(window);
            ball.draw(window);

            // HUD
            std::ostringstream hud;
            hud << "BLUE  " << scoreA << "  :  " << scoreB << "  RED";
            sf::Text hudText(font, hud.str(), 32);
            hudText.setFillColor(sf::Color::White);
            hudText.setPosition({WINDOW_W/2.f - hudText.getGlobalBounds().size.x/2.f, 18.f});
            window.draw(hudText);

            sf::Text escHint(font, "ESC = Menu", 18);
            escHint.setFillColor(sf::Color(200,200,200));
            escHint.setPosition({10.f, 10.f});
            window.draw(escHint);

            window.display();
        }
        backToMenu:;
    }

    return 0;
}