#include "RendererRayTrace.h"

#include "Camera.h"
#include "Shader.h"
#include "Geometry.h"

#include "Window.h"
#include "Scene.h"

#include "Mesh.h"
#include "Entity.h"
#include "Material.h"
#include "Transform.h"

#include "BoundingVolume.h"
#include "Globals.h"

#include "GLUtils.h"
#include "ImGuiUtils.h"

TYvoid RenderRayTrace::PreRender() 
{

}

TYvoid RenderRayTrace::Render(TYfloat dt) 
{
	Layout layout = window->GetLayout();
	TYint width = layout.width;
	TYint height = layout.height;

	camera->Update(dt);

	camera->dim.y = glm::tan(Global::FOV / 2.0f);
	camera->dim.x = camera->dim.y * (TYfloat(width) / height);

	RayTraceShader->Use();

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, SSBO_Sphere);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, SSBO_Triangle);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, SSBO_Model);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, SSBO_Bound);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, SSBO_Surface);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Frame);
	RayTraceShader->Uniforms["frame"](0);
	glBindImageTexture(0, Frame, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

	RayTraceShader->Uniforms["DevI"](Global::DevComputeShaderI);
	RayTraceShader->Uniforms["DevB"](Global::DevComputeShaderB);
	RayTraceShader->Uniforms["DevF"](Global::DevComputeShaderF);
	RayTraceShader->Uniforms["DevV"](Global::DevComputeShaderV);

	RayTraceShader->Uniforms["numLights"](lightCount);
	RayTraceShader->Uniforms["numSpheres"](sphereCount);

	RayTraceShader->Uniforms["FOV"](Global::FOV);
	RayTraceShader->Uniforms["initSeed"](GetRand(0.0f, 1.0f));

	RayTraceShader->Uniforms["voidColor"](TYvec(0.35f, 0.6f, 0.392f));

	RayTraceShader->Uniforms["camPos"](camera->position);
	RayTraceShader->Uniforms["camFront"](camera->front);
	RayTraceShader->Uniforms["camRight"](camera->right);
	RayTraceShader->Uniforms["camUp"](camera->up);
	RayTraceShader->Uniforms["camDim"](camera->dim);

	glDispatchCompute(width, height, 1);
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	// Draw to buffer
	glBindFramebuffer(GL_FRAMEBUFFER, RenderBuffer);
	glViewport(0, 0, layout.width, layout.height);

	glClearColor(0.9f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	BloomShader->Use();
	BloomShader->DrawQuad(Frame);

	ImGui::Begin("Details");

	ImGui::Text("Camera Pos: %.3f, %.3f, %.3f", camera->position.x, camera->position.y, camera->position.z);
	ImGui::Text("Camera Front: %.3f, %.3f, %.3f", camera->front.x, camera->front.y, camera->front.z);

	ImGui::End();
}

TYvoid RenderRayTrace::PostRender() 
{
	QuadShader->Use();
	QuadShader->DrawQuad(RenderTexture);
}

TYvoid RenderRayTrace::RecompileRaytracer()
{
	delete RayTraceShader;
	RayTraceShader = new Shader("raytracer.cs");
}

TYvoid RenderRayTrace::Init()
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

	glGenTextures(1, &Frame);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, Frame);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, layout.width, layout.height, 0, GL_RGBA, GL_FLOAT, NULL);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	TYvector<SPHERE> spheres;
	TYvector<TRIANGLE> triangles;
	TYvector<MODEL> models;
	TYvector<BOUND> bounds;
	TYvector<SURFACE> surfaces;

	// Load scene

	for (Entity* entity : scene->entityList)
	{
		Mesh* mesh = entity->Get<Mesh*>();
		Transform* transform = entity->Get<Transform*>();

		//TYvector<Geometry*> geoList = mesh->GetGeometryList();
		const TYvector<SubMesh>& subMeshList = mesh->GetSubMeshList();

		for (const SubMesh& subMesh : subMeshList)
		{
			Geometry* geo = subMesh.geometry;
			if (geo->GetType() == geoSphere)
			{
				spheres.push_back(SPHERE(geo->center, reinterpret_cast<Sphere*>(geo)->radius));
			}
			else if (geo->GetType() == geoTriangle)
			{
				Triangle* tt = reinterpret_cast<Triangle*>(geo);

				TYvec a, b, c;
				a = tt->vertices[0].position;
				b = tt->vertices[1].position;
				c = tt->vertices[2].position;

				models.push_back(MODEL((TYint)triangles.size(), 1));

				TYvec ce = (a + b + c) / 3.0f;

				TYfloat r = glm::max(glm::length(a - ce), glm::max(glm::length(b - ce), glm::length(c - ce)));

				bounds.push_back(BOUND(ce, r));

				triangles.push_back(TRIANGLE(a, b, c));
			}
			else if (geo->GetType() == geoModel)
			{
				Model* ss = reinterpret_cast<Model*>(geo);
				models.push_back(MODEL((TYint)triangles.size(), (TYint)ss->triangles.size()));

				BoundingSphere* bs = reinterpret_cast<BoundingSphere*>(ss->bvh->head);

				bounds.push_back(BOUND(bs->center + transform->Get<Transformation::Position>(), bs->radius));

				for (Triangle t : ss->triangles)
				{
					TYvec a, b, c;
					a = t.vertices[0].position + transform->Get<Transformation::Position>();
					b = t.vertices[1].position + transform->Get<Transformation::Position>();
					c = t.vertices[2].position + transform->Get<Transformation::Position>();

					triangles.push_back(TRIANGLE(a, b, c));
				}
			}
			else
			{
				continue;
			}

			surfaces.push_back(SURFACE(geo->surfaceColor, geo->transparency, geo->emissionColor, geo->reflection));
		}
	}

	lightCount = 1;
	sphereCount = TYint(spheres.size());

	RayTraceShader->Use();

	// SSBO
	glGenBuffers(1, &SSBO_Sphere);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_Sphere);
	glBufferData(GL_SHADER_STORAGE_BUFFER, spheres.size() * sizeof(SPHERE), spheres.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &SSBO_Triangle);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_Triangle);
	glBufferData(GL_SHADER_STORAGE_BUFFER, triangles.size() * sizeof(TRIANGLE), triangles.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &SSBO_Model);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_Model);
	glBufferData(GL_SHADER_STORAGE_BUFFER, models.size() * sizeof(MODEL), models.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &SSBO_Bound);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_Bound);
	glBufferData(GL_SHADER_STORAGE_BUFFER, bounds.size() * sizeof(BOUND), bounds.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &SSBO_Surface);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, SSBO_Surface);
	glBufferData(GL_SHADER_STORAGE_BUFFER, surfaces.size() * sizeof(SURFACE), surfaces.data(), GL_STATIC_DRAW);
}

RenderRayTrace::RenderRayTrace() : Renderer()
{
	SetType(RendererType::RayTrace);

	Mesh::GenHandles = false;

	RayTraceShader = new Shader("raytracer.cs");
	QuadShader = new Shader("quad.vs", "quad.fs");
	BloomShader = new Shader("bloom.vs", "bloom.fs");

	scene = new Scene(1);
	scene->GenOctree();
}

RenderRayTrace::~RenderRayTrace()
{
	delete RayTraceShader;
	delete QuadShader;
	delete BloomShader;

	glDeleteTextures(1, &RenderTexture);
	glDeleteTextures(1, &Frame);

	glDeleteFramebuffers(1, &RenderBuffer);

	glDeleteBuffers(1, &SSBO_Sphere);
	glDeleteBuffers(1, &SSBO_Triangle);
	glDeleteBuffers(1, &SSBO_Model);
	glDeleteBuffers(1, &SSBO_Bound);
	glDeleteBuffers(1, &SSBO_Surface);
}