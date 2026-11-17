#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

struct SimulationState {
    // Physical parameters
    const float gravity = 500.f;       // Gravity acceleration (px/s^2, downward)
    const float launchAngleDeg = 45.f; // Fixed launch angle (degrees)
    float windAccel = 0.f;             // Horizontal wind acceleration (px/s^2)
    float initialSpeed = 450.f;        // Initial launch speed (px/s)

    // Particle state
    const float radius = 10.f;
    const float startX = 120.f;
    const float floorY = 530.f;
    sf::Vector2f pos{startX, floorY - radius};
    sf::Vector2f vel{0.f, 0.f};

    // Simulation metrics & status
    float flightTime = 0.f;
    float distance = 0.f;
    bool isPaused = false;
    bool isLanded = false;
    std::vector<sf::Vector2f> trajectory;
};

void resetSimulation(SimulationState& sim) {
    const float pi = 3.14159265f;
    float rad = sim.launchAngleDeg * pi / 180.f;

    sim.pos = {sim.startX, sim.floorY - sim.radius};
    sim.vel.x = sim.initialSpeed * std::cos(rad);
    sim.vel.y = -sim.initialSpeed * std::sin(rad); // Negative Y is upward in SFML
    sim.flightTime = 0.f;
    sim.distance = 0.f;
    sim.isLanded = false;
    sim.isPaused = false;

    sim.trajectory.clear();
    sim.trajectory.push_back(sim.pos);
}

// 1. Input handling
void handleInput(sf::RenderWindow& window, SimulationState& sim) {
    while (const std::optional event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
        }
        if (event->is<sf::Event::KeyPressed>()) {
            const auto* key = event->getIf<sf::Event::KeyPressed>();
            if (key->code == sf::Keyboard::Key::Right) {
                sim.windAccel += 50.f;
            }
            if (key->code == sf::Keyboard::Key::Left) {
                sim.windAccel -= 50.f;
            }
            if (key->code == sf::Keyboard::Key::Up) {
                sim.initialSpeed += 50.f;
            }
            if (key->code == sf::Keyboard::Key::Down) {
                sim.initialSpeed -= 50.f;
            }
            if (key->code == sf::Keyboard::Key::Space) {
                if (sim.isLanded) {
                    resetSimulation(sim);
                } else {
                    sim.isPaused = !sim.isPaused;
                }
            }
            if (key->code == sf::Keyboard::Key::R) {
                resetSimulation(sim);
            }

            sim.windAccel = std::clamp(sim.windAccel, -400.f, 300.f);
            sim.initialSpeed = std::clamp(sim.initialSpeed, 200.f, 700.f);
        }
    }
}

// 2. Physical model update
void updatePhysics(SimulationState& sim, float dt) {
    if (sim.isPaused || sim.isLanded) {
        return;
    }

    // Clamp dt to prevent numerical instability
    if (dt > 0.05f) {
        dt = 0.05f;
    }

    // Semi-implicit Euler integration
    sim.vel.x += sim.windAccel * dt;
    sim.vel.y += sim.gravity * dt;

    sim.pos.x += sim.vel.x * dt;
    sim.pos.y += sim.vel.y * dt;

    sim.flightTime += dt;
    sim.distance = sim.pos.x - sim.startX;

    // Ground collision check
    if (sim.pos.y + sim.radius >= sim.floorY) {
        sim.pos.y = sim.floorY - sim.radius;
        sim.distance = sim.pos.x - sim.startX;
        sim.isLanded = true;
    }

    sim.trajectory.push_back(sim.pos);
    if (sim.trajectory.size() > 1000) {
        sim.trajectory.erase(sim.trajectory.begin());
    }
}

// 3. Scene rendering
void drawScene(sf::RenderWindow& window, const SimulationState& sim, const sf::Font& font) {
    window.clear(sf::Color(22, 26, 35));

    // Ground
    sf::RectangleShape ground({800.f, 70.f});
    ground.setPosition({0.f, sim.floorY});
    ground.setFillColor(sf::Color(70, 85, 75));
    window.draw(ground);

    // Launch point marker
    sf::RectangleShape startMark({4.f, 16.f});
    startMark.setPosition({sim.startX - 2.f, sim.floorY - 8.f});
    startMark.setFillColor(sf::Color::Yellow);
    window.draw(startMark);

    // Trajectory
    sf::VertexArray line(sf::PrimitiveType::LineStrip);
    for (const auto& pt : sim.trajectory) {
        line.append(sf::Vertex{pt, sf::Color(180, 210, 255)});
    }
    window.draw(line);

    // Particle
    sf::CircleShape particle(sim.radius);
    particle.setOrigin({sim.radius, sim.radius});
    particle.setPosition(sim.pos);
    particle.setFillColor(sf::Color(255, 95, 85));
    window.draw(particle);

    // Visual wind bar (top-right)
    sf::RectangleShape windBox({220.f, 16.f});
    windBox.setPosition({550.f, 35.f});
    windBox.setFillColor(sf::Color(45, 50, 62));
    windBox.setOutlineThickness(1.f);
    windBox.setOutlineColor(sf::Color(120, 130, 150));
    window.draw(windBox);

    float centerX = 660.f;
    if (sim.windAccel != 0.f) {
        float barWidth = (sim.windAccel / 400.f) * 105.f;
        sf::RectangleShape windBar({std::abs(barWidth), 12.f});
        windBar.setPosition({barWidth > 0 ? centerX : centerX + barWidth, 37.f});
        windBar.setFillColor(barWidth > 0 ? sf::Color(80, 210, 120) : sf::Color(255, 160, 60));
        window.draw(windBar);
    }

    // Text parameters
    std::string windDir = "Calm (0)";
    if (sim.windAccel > 0.f) windDir = "Tailwind (Right ->)";
    else if (sim.windAccel < 0.f) windDir = "Headwind (<- Left)";

    std::string status = sim.isLanded ? "LANDED (Press R to relaunch)"
                       : (sim.isPaused ? "PAUSED (Press Space)" : "RUNNING");

    char buf[512];
    std::snprintf(
        buf, sizeof(buf),
        "Variant 28: Particle in Gravity & Wind\n"
        "Status: %s\n\n"
        "Gravity (g):      %.0f px/s^2\n"
        "Initial Speed:    %.0f px/s  [Up / Down]\n"
        "Wind Accel (aw):  %.0f px/s^2 (%s)  [Left / Right]\n"
        "Current Vel:      (%.1f, %.1f) px/s\n"
        "Flight Time:      %.2f s\n"
        "Distance (dX):    %.1f px\n\n"
        "Controls: [Space] Start/Pause | [R] Restart",
        status.c_str(), sim.gravity, sim.initialSpeed,
        sim.windAccel, windDir.c_str(),
        sim.vel.x, sim.vel.y, sim.flightTime, sim.distance
    );

    sf::Text infoText(font, buf, 16);
    infoText.setFillColor(sf::Color::White);
    infoText.setPosition({20.f, 15.f});
    window.draw(infoText);

    window.display();
}

int main() {
    sf::RenderWindow window(
        sf::VideoMode({800, 600}),
        "Lab 5 - Variant 28: Gravity & Wind"
    );
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("resources/fonts/arial.ttf") &&
        !font.openFromFile("../../resources/fonts/arial.ttf") &&
        !font.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        return 1;
    }

    SimulationState sim;
    resetSimulation(sim);
    sf::Clock clock;

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        handleInput(window, sim);
        updatePhysics(sim, dt);
        drawScene(window, sim, font);
    }

    return 0;
}
