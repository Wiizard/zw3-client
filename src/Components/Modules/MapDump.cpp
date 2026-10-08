#include "STDInclude.hpp"

#include "MapDump.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Logger.hpp"

namespace Components
{
	using D3DXSaveTextureToFileA_t = HRESULT(WINAPI*)(const char* destFile, int destFormat, IDirect3DBaseTexture9* srcTexture, const PALETTEENTRY* srcPalette);

	constexpr int d3dxImageFormatPng = 3;

	class MapDumper
	{
	public:
		explicit MapDumper(Game::GfxWorld* world) : world(world)
		{
		}

		~MapDumper()
		{
			if (this->d3dx)
			{
				FreeLibrary(this->d3dx);
			}
		}

		MapDumper(const MapDumper&) = delete;
		MapDumper& operator=(const MapDumper&) = delete;

		void Dump()
		{
			Logger::Print("Exporting '{}'...\n", this->world->baseName);

			this->d3dx = LoadLibraryA("d3dx9_43.dll");

			if (this->d3dx)
			{
				this->saveTextureToFile = reinterpret_cast<D3DXSaveTextureToFileA_t>(GetProcAddress(this->d3dx, "D3DXSaveTextureToFileA"));
			}

			if (!this->saveTextureToFile)
			{
				Logger::Print("d3dx9_43.dll is not installed, textures are not exported\n");
			}

			this->ParseVertices();
			this->ParseFaces();
			this->ParseStaticModels();

			this->Write();
		}

	private:
		struct Vertex
		{
			float coordinate[3];
			float texture[2];
			float normal[3];
		};

		struct Face
		{
			int a{};
			int b{};
			int c{};
		};

		struct FaceList
		{
			std::vector<Face> indices{};
		};

		Game::GfxWorld* world{};
		std::vector<Vertex> vertices{};
		std::unordered_map<Game::Material*, FaceList> faces{};
		std::vector<Game::Material*> facesOrder{};

		std::ofstream objectFile{};
		std::ofstream materialFile{};

		HMODULE d3dx = nullptr;
		D3DXSaveTextureToFileA_t saveTextureToFile = nullptr;

		static void TransformAxes(float* vec)
		{
			std::swap(vec[0], vec[1]);
			std::swap(vec[1], vec[2]);
		}

		void ParseVertices()
		{
			Logger::Print("Parsing vertices...\n");

			const auto& draw = this->world->draw;

			if (!draw.vd.vertices)
			{
				return;
			}

			for (unsigned int i = 0; i < draw.vertexCount; ++i)
			{
				const auto& vertex = draw.vd.vertices[i];

				Vertex v{};

				v.coordinate[0] = vertex.xyz[0];
				v.coordinate[1] = vertex.xyz[1];
				v.coordinate[2] = vertex.xyz[2];
				TransformAxes(v.coordinate);

				v.texture[0] = vertex.texCoord[0];
				v.texture[1] = -vertex.texCoord[1];

				Game::Vec3UnpackUnitVec(vertex.normal, v.normal);
				TransformAxes(v.normal);

				this->vertices.push_back(v);
			}
		}

		void ParseFaces()
		{
			Logger::Print("Parsing faces...\n");

			const auto& dpvs = this->world->dpvs;

			for (unsigned int i = 0; i < dpvs.staticSurfaceCount; ++i)
			{
				const auto& surface = dpvs.surfaces[i];

				const unsigned int vertOffset = surface.tris.firstVertex + 1;
				const unsigned int indexOffset = surface.tris.baseIndex;

				const auto* colorMap = this->FindImage(surface.material, "colorMap");

				if (colorMap && colorMap->mapType == Game::MAPTYPE_CUBE)
				{
					continue;
				}

				auto& faceList = this->GetFaceList(surface.material);

				for (unsigned short j = 0; j < surface.tris.triCount; ++j)
				{
					Face face{};
					face.a = this->world->draw.indices[indexOffset + j * 3 + 0] + vertOffset;
					face.b = this->world->draw.indices[indexOffset + j * 3 + 1] + vertOffset;
					face.c = this->world->draw.indices[indexOffset + j * 3 + 2] + vertOffset;

					faceList.indices.push_back(face);
				}
			}
		}

		FaceList& GetFaceList(Game::Material* material)
		{
			auto& faceList = this->faces[material];

			if (this->facesOrder.size() < this->faces.size())
			{
				this->facesOrder.push_back(material);
			}

			return faceList;
		}

		static void PerformWorldTransformation(const Game::GfxPackedPlacement& placement, Vertex& v)
		{
			Game::MatrixVecMultiply(placement.axis, v.normal, v.normal);
			Game::Vec3Normalize(v.normal);

			Game::MatrixVecMultiply(placement.axis, v.coordinate, v.coordinate);
			v.coordinate[0] = v.coordinate[0] * placement.scale + placement.origin[0];
			v.coordinate[1] = v.coordinate[1] * placement.scale + placement.origin[1];
			v.coordinate[2] = v.coordinate[2] * placement.scale + placement.origin[2];
		}

		static std::vector<Vertex> ParseSurfaceVertices(const Game::XSurface& surface, const Game::GfxPackedPlacement& placement)
		{
			std::vector<Vertex> surfaceVertices;

			for (unsigned short j = 0; j < surface.vertCount; ++j)
			{
				const auto& vertex = surface.verts0[j];

				Vertex v{};

				v.coordinate[0] = vertex.xyz[0];
				v.coordinate[1] = vertex.xyz[1];
				v.coordinate[2] = vertex.xyz[2];

				Game::Vec2UnpackTexCoords(vertex.texCoord, v.texture);
				std::swap(v.texture[0], v.texture[1]);
				v.texture[1] *= -1;

				Game::Vec3UnpackUnitVec(vertex.normal, v.normal);

				PerformWorldTransformation(placement, v);
				TransformAxes(v.coordinate);
				TransformAxes(v.normal);

				surfaceVertices.push_back(v);
			}

			return surfaceVertices;
		}

		static std::vector<Face> ParseSurfaceFaces(const Game::XSurface& surface)
		{
			std::vector<Face> surfaceFaces;

			for (unsigned short j = 0; j < surface.triCount; ++j)
			{
				Face face{};
				face.a = surface.triIndices[j * 3 + 0];
				face.b = surface.triIndices[j * 3 + 1];
				face.c = surface.triIndices[j * 3 + 2];

				surfaceFaces.push_back(face);
			}

			return surfaceFaces;
		}

		static void RemoveVertex(int index, std::vector<Face>& surfaceFaces, std::vector<Vertex>& surfaceVertices)
		{
			surfaceVertices.erase(surfaceVertices.begin() + index);

			for (auto& face : surfaceFaces)
			{
				if (face.a > index)
				{
					--face.a;
				}

				if (face.b > index)
				{
					--face.b;
				}

				if (face.c > index)
				{
					--face.c;
				}
			}
		}

		static void FilterSurfaceVertices(std::vector<Face>& surfaceFaces, std::vector<Vertex>& surfaceVertices)
		{
			for (int i = 0; i < static_cast<int>(surfaceVertices.size()); ++i)
			{
				bool isReferenced = false;

				for (const auto& face : surfaceFaces)
				{
					if (face.a == i || face.b == i || face.c == i)
					{
						isReferenced = true;
						break;
					}
				}

				if (!isReferenced)
				{
					RemoveVertex(i, surfaceFaces, surfaceVertices);
					--i;
				}
			}
		}

		void ParseStaticModel(const Game::GfxStaticModelDrawInst& drawInst)
		{
			const auto* model = drawInst.model;

			if (!model || !model->numLods)
			{
				return;
			}

			for (unsigned char i = 0; i < model->numsurfs; ++i)
			{
				this->GetFaceList(model->materialHandles[i]);
			}

			const auto& lod = model->lodInfo[model->numLods - 1];

			if (!lod.modelSurfs)
			{
				return;
			}

			assert(lod.modelSurfs->numsurfs <= model->numsurfs);

			for (unsigned short i = 0; i < lod.modelSurfs->numsurfs; ++i)
			{
				const auto& surface = lod.modelSurfs->surfs[i];

				auto surfaceFaces = ParseSurfaceFaces(surface);
				auto surfaceVertices = ParseSurfaceVertices(surface, drawInst.placement);
				FilterSurfaceVertices(surfaceFaces, surfaceVertices);

				const auto baseIndex = static_cast<int>(this->vertices.size()) + 1;
				auto& faceList = this->GetFaceList(model->materialHandles[i + lod.surfIndex]);

				for (const auto& vertex : surfaceVertices)
				{
					this->vertices.push_back(vertex);
				}

				for (auto face : surfaceFaces)
				{
					face.a += baseIndex;
					face.b += baseIndex;
					face.c += baseIndex;
					faceList.indices.push_back(face);
				}
			}
		}

		void ParseStaticModels()
		{
			Logger::Print("Parsing static models...\n");

			for (unsigned int i = 0; i < this->world->dpvs.smodelCount; ++i)
			{
				this->ParseStaticModel(this->world->dpvs.smodelDrawInsts[i]);
			}
		}

		static std::ofstream OpenFile(const std::string& path)
		{
			Utils::IO::WriteFile(path, {});
			return std::ofstream(path, std::ofstream::out);
		}

		void Write()
		{
			const auto* baseName = this->world->baseName;

			this->objectFile = OpenFile(Utils::String::VA("raw/mapdump/%s/%s.obj", baseName, baseName));
			this->materialFile = OpenFile(Utils::String::VA("raw/mapdump/%s/%s.mtl", baseName, baseName));

			this->objectFile << "# Generated by IW4x\n";
			this->objectFile << "# Credit to SE2Dev for his D3DBSP Tool\n";
			this->objectFile << Utils::String::VA("o %s\n", baseName);
			this->objectFile << Utils::String::VA("mtllib %s.mtl\n\n", baseName);

			this->materialFile << "# IW4x MTL File\n";
			this->materialFile << "# Credit to SE2Dev for his D3DBSP Tool\n";

			this->WriteVertices();
			this->WriteFaces();

			Logger::Print("Writing files...\n");

			this->objectFile.close();
			this->materialFile.close();
		}

		void WriteVertices()
		{
			Logger::Print("Writing vertices...\n");
			this->objectFile << "# Vertices\n";

			for (const auto& vertex : this->vertices)
			{
				this->objectFile << Utils::String::VA("v %.6f %.6f %.6f\n", vertex.coordinate[0], vertex.coordinate[1], vertex.coordinate[2]);
			}

			Logger::Print("Writing texture coordinates...\n");
			this->objectFile << "\n# Texture coordinates\n";

			for (const auto& vertex : this->vertices)
			{
				this->objectFile << Utils::String::VA("vt %.6f %.6f\n", vertex.texture[0], vertex.texture[1]);
			}

			Logger::Print("Writing normals...\n");
			this->objectFile << "\n# Normals\n";

			for (const auto& vertex : this->vertices)
			{
				this->objectFile << Utils::String::VA("vn %.6f %.6f %.6f\n", vertex.normal[0], vertex.normal[1], vertex.normal[2]);
			}

			this->objectFile << "\n";
		}

		static Game::GfxImage* FindImage(const Game::Material* material, const char* type)
		{
			if (!material || !material->textureTable)
			{
				return nullptr;
			}

			const auto hash = Game::R_HashString(type);

			for (unsigned char i = 0; i < material->textureCount; ++i)
			{
				if (material->textureTable[i].nameHash == hash)
				{
					return material->textureTable[i].u.image;
				}
			}

			return nullptr;
		}

		Game::GfxImage* ExtractImage(const Game::Material* material, const char* type) const
		{
			auto* image = FindImage(material, type);

			if (!image)
			{
				return nullptr;
			}

			if (this->saveTextureToFile && image->texture.basemap)
			{
				const std::string path = Utils::String::VA("raw/mapdump/%s/textures/%s.png", this->world->baseName, image->name);
				this->saveTextureToFile(path.data(), d3dxImageFormatPng, image->texture.basemap, nullptr);
			}

			return image;
		}

		void WriteMaterial(const Game::Material* material)
		{
			std::string name = material->info.name;

			const auto pos = name.find_last_of('/');

			if (pos != std::string::npos)
			{
				name = name.substr(pos + 1);
			}

			this->objectFile << Utils::String::VA("usemtl %s\n", name.data());
			this->objectFile << "s off\n";

			const auto* colorMap = this->ExtractImage(material, "colorMap");
			const auto* normalMap = this->ExtractImage(material, "normalMap");
			const auto* specularMap = this->ExtractImage(material, "specularMap");

			this->materialFile << Utils::String::VA("\nnewmtl %s\n", name.data());
			this->materialFile << "Ka 1.0000 1.0000 1.0000\n";
			this->materialFile << "Kd 1.0000 1.0000 1.0000\n";
			this->materialFile << "illum 1\n";

			if (colorMap)
			{
				this->materialFile << Utils::String::VA("map_Ka textures/%s.png\n", colorMap->name);
				this->materialFile << Utils::String::VA("map_Kd textures/%s.png\n", colorMap->name);
			}

			if (specularMap)
			{
				this->materialFile << Utils::String::VA("map_Ks textures/%s.png\n", specularMap->name);
			}

			if (normalMap)
			{
				this->materialFile << Utils::String::VA("bump textures/%s.png\n", normalMap->name);
			}
		}

		void WriteFaces()
		{
			Logger::Print("Writing faces...\n");
			Utils::IO::CreateDir(Utils::String::VA("raw/mapdump/%s/textures", this->world->baseName));

			this->materialFile << Utils::String::VA("# Material count: %zu\n", this->faces.size());

			this->objectFile << "# Faces\n";

			for (auto* material : this->facesOrder)
			{
				this->WriteMaterial(material);

				for (const auto& face : this->faces[material].indices)
				{
					this->objectFile << Utils::String::VA("f %d/%d/%d %d/%d/%d %d/%d/%d\n", face.a, face.a, face.a, face.b, face.b, face.b, face.c, face.c, face.c);
				}

				this->objectFile << "\n";
			}
		}
	};

	MapDump::MapDump()
	{
		Command::Add("dumpmap", []
		{
			if (Dedicated::IsEnabled())
			{
				Logger::Print("DirectX needs to be enabled, please start a client to use this command!\n");
				return;
			}

			Game::GfxWorld* world = nullptr;
			Game::DB_EnumXAssets_FastFile(Game::ASSET_TYPE_GFXWORLD, [](void* header, void* data)
			{
				*static_cast<Game::GfxWorld**>(data) = static_cast<Game::GfxWorld*>(header);
			}, &world, false);

			if (!world)
			{
				Logger::Print("No map loaded, unable to dump anything!\n");
				return;
			}

			MapDumper dumper(world);
			dumper.Dump();

			Logger::Print("Map '{}' exported!\n", world->baseName);
		});
	}
}
