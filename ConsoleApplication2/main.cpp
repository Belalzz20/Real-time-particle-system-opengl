#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>   
#include <algorithm>

#include <GL/gl.h>
#include <GL/glu.h>

#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")  

// ================= Window Settings =================
static int WIDTH = 800;
static int HEIGHT = 800;

// ================= Camera =================
static float camYaw = 0.0f;       // left / right
static float camPitch = 20.0f;    // up / down
static float camDist = 2.5f;      // zoom

static bool mouseDown = false;
static int lastMouseX = 0;
static int lastMouseY = 0;

// ================= Particle =================
struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float life, maxLife;
    float angle;
    float spin;  
};

static float sideForce = 0.0f;       
static float sideForceTarget = 0.0f;  


static std::vector<Particle> particles;

// ================= Particle Count Control =================
static int currentParticles = 3000;
static const int MIN_PARTICLES = 500;
static const int MAX_PARTICLES = 10000;

// ================= Globals =================
static HWND  hWnd = nullptr;
static HDC   hDC = nullptr;
static HGLRC hRC = nullptr;
static bool  running = true;

static const float MIN_PARTICLE_RENDER_SIZE = 0.0025f;
static const float MAX_PARTICLE_RENDER_SIZE = 0.012f;

// ================= Particle Size =================
static float particleSize = 2.0f;

// ================= User Twist Control =================
static float angularVel = 0.0f;  
static const float ANGULAR_SPEED = 5.0f;

// ================= Utility =================
static float frand(float a, float b) {
    return a + (b - a) * (rand() / (float)RAND_MAX);
}

static void respawn(Particle& p) {

    p.x = frand(-0.15f, 0.15f);
    p.y = -0.9f;
    p.z = frand(-0.15f, 0.15f);


    p.vx = frand(-0.5f, 0.5f);
    p.vy = frand(0.9f, 1.5f);
    p.vz = frand(-0.5f, 0.5f);

    p.maxLife = frand(1.8f, 3.2f);
    p.life = p.maxLife;

    p.angle = frand(0.0f, 360.0f);
    p.spin = frand(200.0f, 600.0f);
}



// ================= Init =================
static void initParticles() {
    particles.resize(currentParticles);
    for (auto& p : particles) {
        respawn(p);
        p.life *= frand(0.0f, 1.0f);
    }
}

static void applyProjection() {
    glViewport(0, 0, WIDTH, HEIGHT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    double aspect = (HEIGHT > 0) ? (double)WIDTH / (double)HEIGHT : 1.0;
    gluPerspective(60.0, aspect, 0.1, 50.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static void applyCamera() {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float yawRad = camYaw * 0.0174532925f;
    float pitchRad = camPitch * 0.0174532925f;

    float cx = camDist * cosf(pitchRad) * sinf(yawRad);
    float cy = camDist * sinf(pitchRad);
    float cz = camDist * cosf(pitchRad) * cosf(yawRad);

    gluLookAt(
        cx, cy, cz,
        0.0, 0.0, 0.0,
        0.0, 1.0, 0.0
    );
}

static void initOpenGL() {
    applyProjection();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
}

// ================= Update =================
static void update(float dt) {

    sideForce += (sideForceTarget - sideForce) * dt * 6.0f;


    for (auto& p : particles) {

        p.life -= dt;
        if (p.life <= 0.0f) {
            respawn(p);
            continue;
        }

        float t = 1.0f - (p.life / p.maxLife);
        p.x += sideForce * dt * (0.8f + t * 2.5f);

        p.y += p.vy * dt;
        p.x += p.vx * dt * (0.6f + t * 2.2f);
        p.z += p.vz * dt * (0.6f + t * 2.2f);

        if (angularVel != 0.0f) {

            const float baseX = 0.0f;
            const float baseY = -0.9f;
            const float baseZ = 0.0f;

            float dx = p.x - baseX;
            float dz = p.z - baseZ;

            float radiusScale = 0.4f + t * 1.8f;
            float angle = -angularVel * dt * radiusScale;

            float c = cosf(angle);
            float s = sinf(angle);

            float rx = dx * c - dz * s;
            float rz = dx * s + dz * c;

            p.x = baseX + rx;
            p.z = baseZ + rz;
        }

        if (p.y > 1.1f) {
            respawn(p);
            continue;
        }

        p.angle += 160.0f * dt;
        if (p.angle >= 360.0f)
            p.angle -= 360.0f;
    }
}






// ================= Render =================
static void render() {

    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    applyCamera();

    glBegin(GL_QUADS);
    for (const auto& p : particles) {

        float lifeRatio = p.life / p.maxLife;

        float baseSize = particleSize * 0.0045f;
        float size = baseSize * (0.3f + lifeRatio);

        if (size > MAX_PARTICLE_RENDER_SIZE)
            size = MAX_PARTICLE_RENDER_SIZE;
        if (size < MIN_PARTICLE_RENDER_SIZE)
            size = MIN_PARTICLE_RENDER_SIZE;

        float alpha = lifeRatio;
        glColor4f(1.0f, 0.5f * alpha, 0.1f, alpha);

        float rad = p.angle * 0.0174532925f;
        float c = cosf(rad);
        float s = sinf(rad);

        float x1 = -size, y1 = -size;
        float x2 = size, y2 = size;

        float rx1 = x1 * c - y1 * s;
        float ry1 = x1 * s + y1 * c;
        float rx2 = x2 * c - y1 * s;
        float ry2 = x2 * s + y1 * c;
        float rx3 = x2 * c - y2 * s;
        float ry3 = x2 * s + y2 * c;
        float rx4 = x1 * c - y2 * s;
        float ry4 = x1 * s + y2 * c;

        glVertex3f(p.x + rx1, p.y + ry1, p.z);
        glVertex3f(p.x + rx2, p.y + ry2, p.z);
        glVertex3f(p.x + rx3, p.y + ry3, p.z);
        glVertex3f(p.x + rx4, p.y + ry4, p.z);
    }
    glEnd();

    SwapBuffers(hDC);
}






// ================= Win32 =================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_KEYDOWN:

        if (wParam == VK_OEM_PLUS || wParam == VK_ADD) {
            currentParticles = std::min(currentParticles + 500, MAX_PARTICLES);
            initParticles();
            return 0;
        }
        if (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT) {
            currentParticles = std::max(currentParticles - 500, MIN_PARTICLES);
            initParticles();
            return 0;
        }

        if (wParam == VK_UP) {
            particleSize += 0.5f;
            if (particleSize > 10.0f) particleSize = 10.0f;
            return 0;
        }
        if (wParam == VK_DOWN) {
            particleSize -= 0.5f;
            if (particleSize < 1.0f) particleSize = 1.0f;
            return 0;
        }

        if (wParam == VK_RIGHT) { angularVel = +ANGULAR_SPEED; return 0; }
        if (wParam == VK_LEFT) { angularVel = -ANGULAR_SPEED; return 0; }

        if (wParam == 'A') {       
            sideForceTarget = -1.0f;
            return 0;
        }
        if (wParam == 'D') {       
            sideForceTarget = 1.0f;
            return 0;
        }

        return 0;

    case WM_KEYUP:

        if (wParam == VK_RIGHT || wParam == VK_LEFT) {
            angularVel = 0.0f;
            return 0;
        }

        if (wParam == 'A' || wParam == 'D') {
            sideForceTarget = 0.0f;
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        mouseDown = true;
        lastMouseX = LOWORD(lParam);
        lastMouseY = HIWORD(lParam);
        return 0;

    case WM_LBUTTONUP:
        mouseDown = false;
        return 0;

    case WM_MOUSEMOVE:
        if (mouseDown) {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            int dx = x - lastMouseX;
            int dy = y - lastMouseY;

            camYaw += dx * 0.3f;
            camPitch += dy * 0.3f;

            if (camPitch > 89.0f)  camPitch = 89.0f;
            if (camPitch < -89.0f) camPitch = -89.0f;

            lastMouseX = x;
            lastMouseY = y;
        }
        return 0;

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        camDist -= delta * 0.001f;

        if (camDist < 0.8f) camDist = 0.8f;
        if (camDist > 6.0f) camDist = 6.0f;
        return 0;
    }

    case WM_SIZE:
        WIDTH = LOWORD(lParam);
        HEIGHT = HIWORD(lParam);
        if (WIDTH < 1)  WIDTH = 1;
        if (HEIGHT < 1) HEIGHT = 1;
        if (hRC) {
            wglMakeCurrent(hDC, hRC);
            applyProjection();
        }
        return 0;

    case WM_CLOSE:
        running = false;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        running = false;
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static bool createWindowAndContext(HINSTANCE hInstance) {
    const char* CLASS_NAME = "OpenGL_Legacy_Window";

    WNDCLASSA wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassA(&wc);

    hWnd = CreateWindowA(
        CLASS_NAME,
        "OpenGL Legacy Particle Vortex (+/- count, Up/Down size, Left/Right twist, Mouse camera)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100, 100, WIDTH, HEIGHT,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hWnd) return false;

    hDC = GetDC(hWnd);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;

    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);

    hRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hRC);
    return true;
}

// ================= Main =================
int main() {
    srand((unsigned)time(0));

    HINSTANCE hInstance = GetModuleHandleA(nullptr);
    if (!createWindowAndContext(hInstance)) return 1;

    initOpenGL();
    initParticles();

    MSG msg{};
    DWORD last = GetTickCount();

    while (running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        DWORD now = GetTickCount();
        float dt = (now - last) / 1000.0f;
        last = now;
        if (dt > 0.033f) dt = 0.033f;

        update(dt);
        render();
        Sleep(1);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(hRC);
    ReleaseDC(hWnd, hDC);
    return 0;
}