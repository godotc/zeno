#include <zenovis/Camera.h>
#include <zenovis/DepthPass.h>
#include <zenovis/EnvmapManager.h>
#include <zenovis/GraphicsManager.h>
#include <zenovis/IGraphic.h>
#include <zenovis/ReflectivePass.h>
#include <zenovis/Scene.h>
#include <zenovis/ShaderManager.h>
#include <zenovis/opengl/buffer.h>
#include <zenovis/opengl/common.h>
#include <zenovis/opengl/vao.h>
#include <map>

namespace zenovis {

Scene::~Scene() = default;

Scene::Scene()
    : camera(std::make_unique<Camera>()),
      shaderMan(std::make_unique<ShaderManager>()) {

    CHECK_GL(glDepthRangef(0, 30000));
    CHECK_GL(glEnable(GL_BLEND));
    CHECK_GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    CHECK_GL(glEnable(GL_DEPTH_TEST));
    CHECK_GL(glEnable(GL_PROGRAM_POINT_SIZE));
    //CHECK_GL(glEnable(GL_PROGRAM_POINT_SIZE));
    //CHECK_GL(glEnable(GL_POINT_SPRITE));
    //CHECK_GL(glEnable(GL_SAMPLE_COVERAGE));
    //CHECK_GL(glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE));
    //CHECK_GL(glEnable(GL_SAMPLE_ALPHA_TO_ONE));
    CHECK_GL(glEnable(GL_MULTISAMPLE));
    CHECK_GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    CHECK_GL(glPixelStorei(GL_PACK_ALIGNMENT, 1));

    lightCluster = std::make_unique<LightCluster>(this);

    vao = std::make_unique<opengl::VAO>();

    auto version = (const char *)glGetString(GL_VERSION);
    zeno::log_info("OpenGL version: {}", version ? version : "(null)");

    envmapMan = std::make_unique<EnvmapManager>(this);
    graphicsMan = std::make_unique<GraphicsManager>(this);
    mDepthPass = std::make_unique<DepthPass>(this);
    mReflectivePass = std::make_unique<ReflectivePass>(this);

    mReflectivePass->initReflectiveMaps(camera->m_nx, camera->m_ny);

    hudGraphics.push_back(makeGraphicGrid(this));
    hudGraphics.push_back(makeGraphicAxis(this));
    //setup_env_map("Default");
}

std::vector<IGraphic *> Scene::graphics() const {
    std::vector<IGraphic *> gras;
    gras.reserve(graphicsMan->graphics.size());
    for (auto const &[key, val] : graphicsMan->graphics) {
        gras.push_back(val.get());
    }
    return gras;
}

void Scene::drawSceneDepthSafe(bool reflect, bool isDepthPass) {
    auto aspRatio = camera->getAspect();

    //glEnable(GL_BLEND);
    //glBlendFunc(GL_ONE, GL_ONE);
    /* CHECK_GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)); */
    // std::cout<<"camPos:"<<g_camPos.x<<","<<g_camPos.y<<","<<g_camPos.z<<std::endl;
    // std::cout<<"camView:"<<g_camView.x<<","<<g_camView.y<<","<<g_camView.z<<std::endl;
    // std::cout<<"camUp:"<<g_camUp.x<<","<<g_camUp.y<<","<<g_camUp.z<<std::endl;
    //CHECK_GL(glDisable(GL_MULTISAMPLE));
    // CHECK_GL(glClearColor(bgcolor.r, bgcolor.g, bgcolor.b, 0.0f));
    CHECK_GL(glClearColor(camera->bgcolor.r, camera->bgcolor.g, camera->bgcolor.b, 0.0f));
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT));

    float range[] = {camera->m_near, 500, 1000, 2000, 8000, camera->m_far};
    for (int i = 5; i >= 1; i--) {
        CHECK_GL(glClearDepth(1));
        CHECK_GL(glClear(GL_DEPTH_BUFFER_BIT));
        camera->proj = glm::perspective(glm::radians(camera->m_fov), aspRatio,
                                        range[i - 1], range[i]);

        for (auto const &gra : graphics()) {
            gra->draw(reflect, isDepthPass);
        }
        if (!isDepthPass && camera->show_grid) {
            for (auto const &hudgra : hudGraphics) {
                hudgra->draw(false, false);
            }
        }
    }
}

void Scene::fast_paint_graphics() {
    CHECK_GL(glViewport(0, 0, camera->m_nx, camera->m_ny));
    vao->bind();
    camera->m_sample_weight = 1.0f;
    /* CHECK_GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)); */
    CHECK_GL(glClearColor(camera->bgcolor.r, camera->bgcolor.g, camera->bgcolor.b, 0.0f));
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    for (auto const &gra : graphics()) {
        gra->draw(false, false);
    }
    if (camera->show_grid) {
        for (auto const &hudgra : hudGraphics) {
            hudgra->draw(false, false);
        }
        draw_small_axis();
    }
    vao->unbind();
}

void Scene::my_paint_graphics(int samples, bool isDepthPass) {

    CHECK_GL(glViewport(0, 0, camera->m_nx, camera->m_ny));
    vao->bind();
    camera->m_sample_weight = 1.0f / (float)samples;
    drawSceneDepthSafe(false, isDepthPass);
    if (!isDepthPass && camera->show_grid) {
        draw_small_axis();
    }
    vao->unbind();
}

void Scene::draw_small_axis() { /* TODO: implement this */
}

bool Scene::anyGraphicHasMaterial() {
    for (auto const &gra : graphics()) {
        if (gra->hasMaterial())
            return true;
    }
    return false;
}

void Scene::draw(unsigned int target_fbo) {
    if (!anyGraphicHasMaterial()) {
        CHECK_GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target_fbo));
        fast_paint_graphics();
        return;
    }

    //CHECK_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    /* ZHXX DONT MODIFY ME */
    mDepthPass->paint_graphics(target_fbo);
}

std::vector<char> Scene::record_frame_offline(int hdrSize, int rgbComps) {
    auto hdrType = std::map<int, int>{
        {1, GL_UNSIGNED_BYTE},
        {2, GL_HALF_FLOAT},
        {4, GL_FLOAT},
    }.at(hdrSize);
    auto rgbType = std::map<int, int>{
        {1, GL_RED},
        {2, GL_RG},
        {3, GL_RGB},
        {4, GL_RGBA},
    }.at(rgbComps);

    std::vector<char> pixels(camera->m_nx * camera->m_ny * rgbComps * hdrSize);

    GLuint fbo, rbo1, rbo2;
    CHECK_GL(glGenRenderbuffers(1, &rbo1));
    CHECK_GL(glGenRenderbuffers(1, &rbo2));
    CHECK_GL(glBindRenderbuffer(GL_RENDERBUFFER, rbo1));
    CHECK_GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, camera->m_nx,
                                   camera->m_ny));
    CHECK_GL(glBindRenderbuffer(GL_RENDERBUFFER, rbo2));
    CHECK_GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F,
                                   camera->m_nx, camera->m_ny));

    CHECK_GL(glGenFramebuffers(1, &fbo));
    CHECK_GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
    CHECK_GL(glFramebufferRenderbuffer(
        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo1));
    CHECK_GL(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                       GL_RENDERBUFFER, rbo2));
    CHECK_GL(glDrawBuffer(GL_COLOR_ATTACHMENT0));
    CHECK_GL(glClearColor(camera->bgcolor.r, camera->bgcolor.g,
                          camera->bgcolor.b, 0.0f));
    CHECK_GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    draw(fbo);

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        CHECK_GL(glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo));
        CHECK_GL(glBlitFramebuffer(0, 0, camera->m_nx, camera->m_ny, 0, 0,
                                   camera->m_nx, camera->m_ny, GL_COLOR_BUFFER_BIT,
                                   GL_NEAREST));
        CHECK_GL(glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo));
        CHECK_GL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
        CHECK_GL(glReadBuffer(GL_COLOR_ATTACHMENT0));

        CHECK_GL(glReadPixels(0, 0, camera->m_nx, camera->m_ny, rgbType,
                              hdrType, pixels.data()));
    }

    CHECK_GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
    CHECK_GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    CHECK_GL(glDeleteRenderbuffers(1, &rbo1));
    CHECK_GL(glDeleteRenderbuffers(1, &rbo2));
    CHECK_GL(glDeleteFramebuffers(1, &fbo));

    return pixels;
}

} // namespace zenovis