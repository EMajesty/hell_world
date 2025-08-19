#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>

#include <SDL2/SDL.h>
#include <GL/gl.h>

// #include "TriangulatePolygon.h"

using namespace std;

struct TBFace
{
    float x1, y1, z1; 
    float x2, y2, z2;
    float x3, y3, z3;
    float ux, uy, uz, uOffset;
    float vx, vy, vz, vOffset;
    float rotation, uScale, vScale;
    // int surfaceFlag = 0;
    // int contentsFlag = 0;
    // int value = 0;
    string textureName;
};

struct TBBrush
{
    vector<TBFace> faces;
};

struct TBEntity
{
    map<string, string> keyvalues;
    vector<TBBrush> brushes;
};

struct TBMap
{
    vector<TBEntity> entities;
};

struct Plane {
    glm::vec3 normal;
    float d;
    Plane(const glm::vec3& n, float dist) : normal(glm::normalize(n)), d(dist) {}
    Plane(const glm::vec3& p1, const glm::vec3& p2, const glm::vec3& p3) {
        normal = glm::normalize(glm::cross(p2 - p1, p3 - p1));
        d = -glm::dot(normal, p1);
    }
};

struct Triangle {
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
    Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
        : v0(a), v1(b), v2(c) {}
};

bool Intersect(const Plane& a, const Plane& b, const Plane& c, glm::vec3& out) {
    glm::vec3 n1 = a.normal, n2 = b.normal, n3 = c.normal;
    float denom = glm::dot(n1, glm::cross(n2, n3));
    if (fabs(denom) < 1e-6f) return false; // Nearly parallel, no valid intersection
    out = (
        -a.d * glm::cross(n2, n3)
        -b.d * glm::cross(n3, n1)
        -c.d * glm::cross(n1, n2)
    ) / denom;
    return true;
}

std::vector<glm::vec3> GetFacePolygon(const std::vector<Plane>& planes, int faceIndex) {
    const auto& face = planes[faceIndex];
    std::vector<glm::vec3> verts;
    for (size_t i = 0; i < planes.size(); ++i) {
        if (i == faceIndex) continue;
        for (size_t j = i + 1; j < planes.size(); ++j) {
            if (j == faceIndex) continue;
            glm::vec3 p;
            if (Intersect(face, planes[i], planes[j], p)) {
                bool inside = true;
                for (size_t k = 0; k < planes.size(); ++k) {
                    if (k == faceIndex) continue;
                    if (glm::dot(planes[k].normal, p) + planes[k].d > 0.01f) { // not inside volume
                        inside = false; break;
                    }
                }
                if (inside)
                    verts.push_back(p);
            }
        }
    }
    // Sort verts CCW around face.normal for a well-formed polygon (implement as needed)
    return verts;
}

void TriangulateFace(const std::vector<glm::vec3>& verts, std::vector<Triangle>& outTris) {
    for (size_t i = 1; i+1 < verts.size(); ++i) {
        outTris.push_back(Triangle(verts, verts[i], verts[i+1]));
    }
}

string trim(const string& s)
{
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

bool parseKeyValue(const string& line, string& key, string& value)
{
    auto firstQuote = line.find('\"');
    if (firstQuote == string::npos) return false;
    auto secondQuote = line.find('\"', firstQuote + 1);
    if (secondQuote == string::npos) return false;
    auto thirdQuote = line.find('\"', secondQuote + 1);
    if (thirdQuote == string::npos) return false;
    auto fourthQuote = line.find('\"', thirdQuote + 1);
    if (fourthQuote == string::npos) return false;

    key = line.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    value = line.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);

    return true;
}

bool getNextToken(istringstream& ss, string& token)
{
    while (ss >> token) {
        if (token == "(" || token == ")" || token == "[" || token == "]")
            continue; // skip these things
        break; // good token found
    }
    return !token.empty();
}

bool parseBrushFace(const string& line, TBFace& face)
{
    istringstream ss(line);
    string token;

    for (int i = 0; i < 3; ++i)
    {
        // getNextToken(ss, token); // '('
        if (!getNextToken(ss, token)) return false;
        if (i == 0) face.x1 = stof(token);
        if (i == 1) face.x2 = stof(token);
        if (i == 2) face.x3 = stof(token);

        if (!getNextToken(ss, token)) return false;
        if (i == 0) face.y1 = stof(token);
        if (i == 1) face.y2 = stof(token);
        if (i == 2) face.y3 = stof(token);

        if (!getNextToken(ss, token)) return false;
        if (i == 0) face.z1 = stof(token);
        if (i == 1) face.z2 = stof(token);
        if (i == 2) face.z3 = stof(token);

        // getNextToken(ss, token); // ');
    }

    if (!getNextToken(ss, token)) return false;
    face.textureName = token;

    // getNextToken(ss, token); // '['
    if (!getNextToken(ss, token)) return false; face.ux = stof(token);
    if (!getNextToken(ss, token)) return false; face.uy = stof(token);
    if (!getNextToken(ss, token)) return false; face.uz = stof(token);
    if (!getNextToken(ss, token)) return false; face.uOffset = stof(token);
    // getNextToken(ss, token); // ']'

    // getNextToken(ss, token); // '['
    if (!getNextToken(ss, token)) return false; face.vx = stof(token);
    if (!getNextToken(ss, token)) return false; face.vy = stof(token);
    if (!getNextToken(ss, token)) return false; face.vz = stof(token);
    if (!getNextToken(ss, token)) return false; face.vOffset = stof(token);
    // getNextToken(ss, token); // ']'

    if (!getNextToken(ss, token)) return false; face.rotation = stof(token);
    if (!getNextToken(ss, token)) return false; face.uScale   = stof(token);
    if (!getNextToken(ss, token)) return false; face.vScale   = stof(token);

    // if (getNextToken(ss, token)) face.surfaceFlag = stoi(token);
    // if (getNextToken(ss, token)) face.contentsFlag = stoi(token);
    // if (getNextToken(ss, token)) face.value = stoi(token);
    return true;
}

TBMap readMap(const string& filename)
{
    ifstream infile(filename);
    string line;
    TBMap map;

    TBEntity* currEntity = nullptr;
    TBBrush* currBrush = nullptr;
    bool inEntity = false;
    bool inBrush = false;

    while (getline(infile, line))
    {
        line = trim(line);

        if (line.empty() || line[0] == '/' || line.substr(0, 2) == "//")
            continue;

        if (line == "{") {
            if (!inEntity) {
                map.entities.emplace_back();
                currEntity = &map.entities.back();
                inEntity = true;
            } else if (inEntity && !inBrush) {
                currEntity->brushes.emplace_back();
                currBrush = &currEntity->brushes.back();
                inBrush = true;
            }
        } else if (line == "}") {
            if (inBrush) {
                currBrush = nullptr;
                inBrush = false;
            } else if (inEntity) {
                currEntity = nullptr;
                inEntity = false;
            }
        } else {
            if (inBrush && currBrush) {
                TBFace face;
                if (parseBrushFace(line, face)) {
                    currBrush->faces.push_back(face);
                }
            } else if (inEntity && currEntity) {
                string key, value;
                if (parseKeyValue(line, key, value)) {
                    currEntity->keyvalues[key] = value;
                }
            }
        }
    }
    return map;
}

int main(int argc, char* argv[])
{
    SDL_Init(SDL_INIT_VIDEO);

    // Set OpenGL attributes (optional, e.g. version, double buffering)
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    // SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window *window = SDL_CreateWindow(
        "OpenGL with SDL2", 
        SDL_WINDOWPOS_CENTERED, 
        SDL_WINDOWPOS_CENTERED, 
        800, 600, 
        SDL_WINDOW_OPENGL
    );

    // TBMap map = readMap("unnamed.map");

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    // vsync??
    SDL_GL_SetSwapInterval(1);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    int running = 1;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = 0;
        }

        // OpenGL drawing
        glClearColor(0.2f, 0.2f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glBegin(GL_TRIANGLES);
        glColor3f(1.f, 0.f, 0.f); glVertex2f(-0.5f, -0.5f);
        glColor3f(0.f, 1.f, 0.f); glVertex2f(0.5f, -0.5f);
        glColor3f(0.f, 0.f, 1.f); glVertex2f(0.0f, 0.5f);
        glEnd();

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

