#ifdef _WIN32
#include <windows.h>
#include <SDL_main.h>
#include <random>
#endif

#include <glm/glm.hpp>
#include <string>
#include <fstream>
#include <vector>
#include <cstdlib>
#include "Canis/Canis.hpp"
#include "Canis/Entity.hpp"
#include "Canis/Graphics.hpp"
#include "Canis/Window.hpp"
#include "Canis/Shader.hpp"
#include "Canis/Debug.hpp"
#include "Canis/IOManager.hpp"
#include "Canis/InputManager.hpp"
#include "Canis/Camera.hpp"
#include "Canis/Model.hpp"
#include "Canis/World.hpp"
#include "Canis/Editor.hpp"
#include "Canis/FrameRateManager.hpp"

using namespace glm;

// git restore .
// git fetch
// git pull

// 3d array
std::vector<std::vector<std::vector<unsigned int>>> map = {};

// --- Fire animation globals ---
static std::vector<Canis::GLTexture> g_fireTextures;
static float g_fireFrameTime    = 0.8f;   // seconds per animation frame
static float g_fireAccumulator  = 0.0f;
static int   g_fireCurrentFrame = 0;

// --- Fire light flicker globals ---
static std::vector<Canis::PointLight> g_fireLights;
static float g_lightFlickerSpeed       = 10.0f; // how fast flicker oscillates

// Animate both texture and light flicker
void AnimateFire(Canis::World &world, Canis::Entity &entity, float deltaTime)
{
    // --- Texture flipbook ---
    g_fireAccumulator += deltaTime;
    if (g_fireAccumulator >= g_fireFrameTime)
    {
        g_fireAccumulator -= g_fireFrameTime;
        g_fireCurrentFrame = (g_fireCurrentFrame + 1) % (int)g_fireTextures.size();
        entity.albedo = &g_fireTextures[g_fireCurrentFrame];
    }

    // --- Light flicker (per-light) ---
    for (auto &light : g_fireLights)
    {
        // simple flicker: sine wave + random jitter
        float base    = 1.0f;
        float jitter  = (rand() % 100) / 100.0f * 0.3f - 0.15f; // ±15%
        float flicker = base + jitter;
        light.diffuse = vec3(flicker);
    }

    // Note: World must reference g_fireLights by reference or pointer.
}

// declaring functions
void SpawnLights(Canis::World &_world);
void LoadMap(std::string _path);
void Rotate(Canis::World &_world, Canis::Entity &_entity, float _deltaTime);

#ifdef _WIN32
#define main SDL_main
extern "C" int main(int argc, char* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    Canis::Init();
    Canis::InputManager inputManager;
    Canis::FrameRateManager frameRateManager;
    frameRateManager.Init(60);



    /// SETUP WINDOW
    Canis::Window window;
    window.MouseLock(true);

    unsigned int flags = 0;

    if (Canis::GetConfig().fullscreen)
        flags |= Canis::WindowFlags::FULLSCREEN;

    window.Create("Hello Graphics", Canis::GetConfig().width, Canis::GetConfig().heigth, flags);
    /// END OF WINDOW SETUP

    Canis::World world(&window, &inputManager, "assets/textures/lowpoly-skybox/");
    SpawnLights(world);

    Canis::Editor editor(&window, &world, &inputManager);

    Canis::Graphics::EnableAlphaChannel();
    Canis::Graphics::EnableDepthTest();

    /// SETUP SHADER
    Canis::Shader shader;
    shader.Compile("assets/shaders/hello_shader.vs", "assets/shaders/hello_shader.fs");
    shader.AddAttribute("aPosition");
    shader.Link();
    shader.Use();
    shader.SetInt("MATERIAL.diffuse", 0);
    shader.SetInt("MATERIAL.specular", 1);
    shader.SetFloat("MATERIAL.shininess", 64);
    shader.SetBool("WIND", false);
    shader.UnUse();

    Canis::Shader grassShader;
    grassShader.Compile("assets/shaders/hello_shader.vs", "assets/shaders/hello_shader.fs");
    grassShader.AddAttribute("aPosition");
    grassShader.Link();
    grassShader.Use();
    grassShader.SetInt("MATERIAL.diffuse", 0);
    grassShader.SetInt("MATERIAL.specular", 1);
    grassShader.SetFloat("MATERIAL.shininess", 64);
    grassShader.SetBool("WIND", true);
    grassShader.SetFloat("WINDEFFECT", 0.2);
    grassShader.UnUse();
    /// END OF SHADER

    /// Load Image
    Canis::GLTexture glassTexture = Canis::LoadImageGL("assets/textures/glass.png", true);
    Canis::GLTexture grassTexture = Canis::LoadImageGL("assets/textures/grass.png", false);
    Canis::GLTexture textureSpecular = Canis::LoadImageGL("assets/textures/container2_specular.png", true);
    Canis::GLTexture cobbleStoneTexture = Canis::LoadImageGL("assets/textures/cobblestone.png", true);
    Canis::GLTexture oaklogTexture = Canis::LoadImageGL("assets/textures/oak_log.png", true);
    Canis::GLTexture oakplanksTexture = Canis::LoadImageGL("assets/textures/oak_planks.png", true);
    Canis::GLTexture grasssideTexture = Canis::LoadImageGL("assets/textures/grass_block_side.png", true);
    Canis::GLTexture grasstopTexture = Canis::LoadImageGL("assets/textures/grass_block_top.png", true);
    Canis::GLTexture stoneTexture = Canis::LoadImageGL("assets/textures/stone.png", true);

for (int i = 1; i <= 5; ++i)
        g_fireTextures.push_back(Canis::LoadImageGL("assets/textures/fire_" + std::to_string(i) + ".png", true));
    /// End of Image Loading

    /// Load Models
    Canis::Model cubeModel = Canis::LoadModel("assets/models/cube.obj");
    Canis::Model grassModel = Canis::LoadModel("assets/models/plants.obj");
    Canis::Model fireModel = Canis::LoadModel("assets/models/fire.obj");
    /// END OF LOADING MODEL

    // Load Map into 3d array
    LoadMap("assets/maps/level.map");
    
    // Loop map and spawn objects
    for (int y = 0; y < map.size(); y++)
    {
        for (int x = 0; x < map[y].size(); x++)
        {
            for (int z = 0; z < map[y][x].size(); z++)
            {
                Canis::Entity entity;
                entity.active = true;

                switch (map[y][x][z])
                {
                case 1: // places a grass top block
                    entity.tag = "stone";
                    entity.albedo = &stoneTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;
                case 2: // places a stone block
                    entity.tag = "grass_top";
                    entity.albedo = &grasstopTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;                    
                case 3: // places a grass block
                    entity.tag = "grass";
                    entity.albedo = &grassTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &grassModel;
                    entity.shader = &grassShader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    entity.Update = &Rotate;
                    world.Spawn(entity);
                    break;
                case 4: // places a cobblestone block
                    entity.tag = "cobblestone";
                    entity.albedo = &cobbleStoneTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;
                case 5: // places a oak log block
                    entity.tag = "oak_log";
                    entity.albedo = &oaklogTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;
                case 6: // places a oak planks block
                    entity.tag = "oak_planks";
                    entity.albedo = &oakplanksTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;
                case 7: // places a glass block
                    entity.tag = "glass";
                    entity.albedo = &glassTexture;
                    entity.specular = &textureSpecular;
                    entity.model = &cubeModel;
                    entity.shader = &shader;
                    entity.transform.position = vec3(x + 0.0f, y + 0.0f, z + 0.0f);
                    world.Spawn(entity);
                    break;
	     case 8: // animated fire + flickering light
        {
            // Fire entity
            Canis::Entity fireEnt;
            fireEnt.active = true;
            fireEnt.tag    = "fire";
            fireEnt.albedo = &g_fireTextures[0];
            fireEnt.specular = &textureSpecular;
            fireEnt.model    = &fireModel;
            fireEnt.shader   = &shader;
            fireEnt.transform.position = vec3(x, y, z);
            fireEnt.Update   = &AnimateFire;
            world.Spawn(fireEnt);

            // Corresponding point light for flicker
            Canis::PointLight pl;
            pl.position   = vec3(x, y + 0.5f, z);
            pl.ambient    = vec3(0.2f, 0.1f, 0.0f);
            pl.diffuse    = vec3(1.0f, 0.5f, 0.2f);
            pl.specular   = vec3(1.0f);
            pl.constant   = 1.0f;
            pl.linear     = 0.07f;
            pl.quadratic  = 0.017f;
            world.SpawnPointLight(pl);
            g_fireLights.push_back(pl);
        }
            break;
                default:
                    break;
                }
            }
        }
    }

    double deltaTime = 0.0;
    double fps = 0.0;

    // Application loop
    while (inputManager.Update(Canis::GetConfig().width, Canis::GetConfig().heigth))
    {
        deltaTime = frameRateManager.StartFrame();
        Canis::Graphics::ClearBuffer(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

        world.Update(deltaTime);
        world.Draw(deltaTime);

        editor.Draw();

        window.SwapBuffer();

        // EndFrame will pause the app when running faster than frame limit
        fps = frameRateManager.EndFrame();

        Canis::Log("FPS: " + std::to_string(fps) + " DeltaTime: " + std::to_string(deltaTime));
    }

    return 0;
}

void Rotate(Canis::World &_world, Canis::Entity &_entity, float _deltaTime)
{
    //_entity.transform.rotation.y += _deltaTime;
}

void LoadMap(std::string _path)
{
    std::ifstream file;
    file.open(_path);

    if (!file.is_open())
    {
        printf("file not found at: %s \n", _path.c_str());
        exit(1);
    }

    int number = 0;
    int layer = 0;

    map.push_back(std::vector<std::vector<unsigned int>>());
    map[layer].push_back(std::vector<unsigned int>());

    while (file >> number)
    {
        if (number == -2) // add new layer
        {
            layer++;
            map.push_back(std::vector<std::vector<unsigned int>>());
            map[map.size() - 1].push_back(std::vector<unsigned int>());
            continue;
        }

        if (number == -1) // add new row
        {
            map[map.size() - 1].push_back(std::vector<unsigned int>());
            continue;
        }

        map[map.size() - 1][map[map.size() - 1].size() - 1].push_back((unsigned int)number);
    }
}

void SpawnLights(Canis::World &_world)
{
    Canis::DirectionalLight directionalLight;
    _world.SpawnDirectionalLight(directionalLight);

    Canis::PointLight pointLight;
    pointLight.position = vec3(0.0f);
    pointLight.ambient = vec3(0.2f);
    pointLight.diffuse = vec3(0.5f);
    pointLight.specular = vec3(1.0f);
    pointLight.constant = 1.0f;
    pointLight.linear = 0.09f;
    pointLight.quadratic = 0.032f;

    _world.SpawnPointLight(pointLight);

    pointLight.position = vec3(0.0f, 0.0f, 1.0f);
    pointLight.ambient = vec3(4.0f, 0.0f, 0.0f);

    _world.SpawnPointLight(pointLight);

    pointLight.position = vec3(-2.0f);
    pointLight.ambient = vec3(0.0f, 4.0f, 0.0f);

    _world.SpawnPointLight(pointLight);

    pointLight.position = vec3(2.0f);
    pointLight.ambient = vec3(0.0f, 0.0f, 4.0f);

    _world.SpawnPointLight(pointLight);
}


