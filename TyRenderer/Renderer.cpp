#include "Renderer.h"

#include "Window.h"
#include "Scene.h"

#include "GenericDraw.h"
#include "Camera.h"
#include "Shader.h"

#include "GLUtils.h"
#include "Utils.h"
#include "Globals.h"

TYvoid Renderer::PreRender()
{

}

TYvoid Renderer::Render(TYfloat dt)
{
	Layout layout = window->GetLayout();
	TYint width = layout.width;
	TYint height = layout.height;

	camera->Update(dt);
}

TYvoid Renderer::PostRender()
{
	QuadShader->Use();
	QuadShader->DrawQuad(RenderTexture);
}

TYvoid Renderer::Init()
{
	Layout layout = window->GetLayout();

	TYint width = layout.width;
	TYint height = layout.height;

	camera = new Camera(true);

	// Setup frameBuffer
	glGenFramebuffers(1, &RenderBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, RenderBuffer);

	// Setup render texture
	glGenTextures(1, &RenderTexture);
	glBindTexture(GL_TEXTURE_2D, RenderTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, RenderTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cout << "ERROR::FRAMEBUFFER:: Framebuffer in Raytrace Renderer is not complete!" << std::endl;
	}
}

Renderer::Renderer()
{
	SetType(RendererType::Base);
}

Renderer::~Renderer()
{
	delete camera;
	delete scene;
}