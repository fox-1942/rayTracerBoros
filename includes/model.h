/* Copyright:
 *
 * LearnOpenGl.com Joey de Vries
 * https://learnopengl.com/Model-Loading/Model
 * https://twitter.com/JoeyDeVriez.
 *
 * Changes were made by Tamás Boros.
 *
 * Attribution-NonCommercial 4.0 International (CC BY-NC 4.0), Creative Commons
*/


#ifndef MODEL_H
#define MODEL_H

#include <GL/glew.h>

#include "../Vendor/glm/glm.hpp"
#include "../Vendor/glm/gtc/matrix_transform.hpp"

#include <assimp/Importer.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "../Vendor/SOIL2.h"
#include "mesh.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

GLint TextureFromFile(const char *path, string directory);

class Model {

public:
    const aiScene *scene;
    unsigned int offset = 0;
    /*  Model Data  */
    vector<Mesh> meshes;
    string directory;
    vector<Texture> textures_loaded;    // Stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
    Material mat;

public:
    vector<glm::vec4> allPositionVertices;
    vector<glm::vec4> indicesPerFaces;
    vector<Material> materials;

    /*  Functions   */
    // Constructor, expects a filepath to a 3D model.
    Model() {}

    Model(string path) {
        this->loadModel(path);

    }

    // Draws the model, and thus all its meshes
    void Draw(ShaderProgram shader) {
        for (GLuint i = 0; i < this->meshes.size(); i++) {
            this->meshes[i].Draw(shader);
        }
    }

    void fillAllPositionVertices() {
        glm::vec4 vectorTemp;
        for (int i = 0; i < meshes.size(); i++) {

            for (int j = 0; j < meshes.at(i).vertices.size(); j++) {
                vectorTemp.x = meshes.at(i).vertices.at(j).Position.x;
                vectorTemp.y = meshes.at(i).vertices.at(j).Position.y;
                vectorTemp.z = meshes.at(i).vertices.at(j).Position.z;
                vectorTemp.w = 1;
                allPositionVertices.push_back(vectorTemp);
            }
        }
    }

private:

    /*  Functions   */
    // Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel(string path) {
        // Read file via ASSIMP
        Assimp::Importer importer;
        scene = importer.ReadFile(path, aiProcess_Triangulate);

        // Check for errors
        if (!scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << endl;
            return;
        }
        // Retrieve the directory path of the filepath
        this->directory = path.substr(0, path.find_last_of('/'));

        // Process ASSIMP's root node recursively
        this->processNode(scene->mRootNode, scene);



        fillAllPositionVertices();
        getInfoAboutModel();
    }

    void getInfoAboutModel() {
        int size = 0;
        for (int i = 0; i < meshes.size(); i++) {
            size += meshes.at(i).vertices.size();
        }
        cout << "Number of meshes in the model: " << meshes.size() << endl;
        cout << "Number of vertices in the model: " << size << endl;
        cout << "Number of faces in the model: " << indicesPerFaces.size() << "\n" << endl;
    }

    // Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode(aiNode *node, const aiScene *scene) {
        // Process each mesh located at the current node
        for (GLuint i = 0; i < node->mNumMeshes; i++) {
            // The node object only contains indices to index the actual objects in the scene.
            // The scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];

            this->meshes.push_back(this->processMesh(mesh, scene));
        }

        // After we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for (GLuint i = 0; i < node->mNumChildren; i++) {
            this->processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene) {
        // Data to fill
        vector<Vertex> vertices;
        vector<GLuint> indices;
        vector<Texture> textures;

        // Walk through each of the mesh's vertices
        for (GLuint i = 0; i < mesh->mNumVertices; i++) {
            Vertex vertex;
            glm::vec3 vector; // We declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.

            // Positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            // Normals
            /* vector.x = mesh->mNormals[i].x;
             vector.y = mesh->mNormals[i].y;
             vector.z = mesh->mNormals[i].z;
             vertex.Normal = vector;
 */
            // Texture Coordinates
            if (mesh->mTextureCoords[0]) // Does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // A vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);
        }


        // Process materials
        if (mesh->mMaterialIndex >= 0) {
            aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];


            aiColor3D color;
            float d;
            float shadingModel;
            float shininess;
            // Read mtl file vertex data

            material->Get(AI_MATKEY_COLOR_AMBIENT, color);
            mat.Ka = glm::vec4(color.r, color.g, color.b, 1.0);

            material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
            mat.Kd = glm::vec4(color.r, color.g, color.b, 1.0);

            material->Get(AI_MATKEY_COLOR_SPECULAR, color);
            mat.Ks = glm::vec4(color.r, color.g, color.b, 1.0);

            material->Get(AI_MATKEY_REFRACTI, d);
            mat.Ni = d;

            material->Get(AI_MATKEY_SHADING_MODEL, shadingModel);
            mat.shadingModel = shadingModel - 1;  //Assimp problem to read illumination model

            material->Get(AI_MATKEY_SHININESS, shininess);
            mat.shininess = shininess;  //Assimp problem to read illumination model

            materials.push_back(mat);

            // We assume a convention for sampler names in the shaders. Each diffuse texture should be named
            // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
            // Same applies to other texture as the following list summarizes:
            // Diffuse: texture_diffuseN
            // Specular: texture_specularN
            // Normal: texture_normalN

            // 1. Diffuse maps
            vector<Texture> diffuseMaps = this->loadMaterialTextures(material, aiTextureType_DIFFUSE,
                                                                     "texture_diffuse");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

            // 2. Specular maps
            vector<Texture> specularMaps = this->loadMaterialTextures(material, aiTextureType_SPECULAR,
                                                                      "texture_specular");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
        }


        // Now walk through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for (GLuint i = 0; i < mesh->mNumFaces; i++) {
            aiFace face = mesh->mFaces[i];
            // Retrieve all indices of the face and store them in the indices vector

            glm::vec4 vec3FacePlusMatIndex;

            vec3FacePlusMatIndex.x = face.mIndices[0] + offset;
            vec3FacePlusMatIndex.y = face.mIndices[1] + offset;
            vec3FacePlusMatIndex.z = face.mIndices[2] + offset;
            vec3FacePlusMatIndex.w = materials.size() - 1;
            indicesPerFaces.push_back(vec3FacePlusMatIndex);

            for (GLuint j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }

        offset += mesh->mNumVertices; //Need to renumber the indices when all vertices are stored in one vertex buffer. Increasing the current index by the offset of the number of vertices.

        // Return a mesh object created from the extracted mesh data
        return Mesh(vertices, indices, textures, mat);
    }

    // Checks all material textures of a given type and loads the textures if they're not loaded yet.
    // The required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, string typeName) {
        vector<Texture> textures;

        for (GLuint i = 0; i < mat->GetTextureCount(type); i++) {
            aiString str;
            mat->GetTexture(type, i, &str);

            // Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            GLboolean skip = false;

            for (GLuint j = 0; j < textures_loaded.size(); j++) {
                if (textures_loaded[j].path == str) {
                    textures.push_back(textures_loaded[j]);
                    skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)

                    break;
                }
            }

            if (!skip) {   // If texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile(str.C_Str(), this->directory);
                texture.type = typeName;
                texture.path = str;
                textures.push_back(texture);

                this->textures_loaded.push_back(
                        texture);  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }

        return textures;
    }
};

GLint TextureFromFile(const char *path, string directory) {
    //Generate texture ID and load texture data

    string filename = string(path);
    filename = directory + '/' + filename;
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height;
    unsigned char *image = SOIL_load_image(filename.c_str(), &width, &height, 0, SOIL_LOAD_RGB);

    // Assign texture to ID
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    SOIL_free_image_data(image);

    return textureID;
}

#endif






/*
        ofstream myfile;
        myfile.open ("../model/vertices.txt");
         for (int i = 0; i < allPositionVertices.size(); i++) {
             myfile << allPositionVertices.at(i).x << " " << allPositionVertices.at(i).y << " "
                    << allPositionVertices.at(i).z << endl;
           }*/
/*
          for(int i=0;i<arr.size();i++){
              for(int j=0;j<arr.at(i).size();j++){
                  cout<<arr.at(i).at(j).x<<" "<<arr.at(i).at(j).y<<" "<<arr.at(i).at(j).z<<endl;
              }
          } */

/* for(int i=0;i<meshes.size();i++){
     for(int j=0;j<meshes.at(i).indices.size()-1;j++){
         if(meshes.at(i).indices.at(j)>meshes.at(i).indices.at(j+1)){
           cout<<"megvan"<<endl;
         }
         //cout<< meshes.at(i).indices.at(j)<< endl;
     }
     cout << endl;
 }*/
/*
        ofstream myfile2;
        myfile2.open ("../model/indicesperFaces.txt");
        for(int i=0;i<indicesPerFaces.size();i++){
            myfile2<<indicesPerFaces.at(i).x <<" ";
            myfile2<<indicesPerFaces.at(i).y <<" ";
            myfile2<<indicesPerFaces.at(i).z << endl;
        }*/


/*
          ofstream myfile2;
           myfile2.open ("../model/originalindices.txt");

           for(int i=0;i<meshes.size();i++) {
               int db=0;
               for(int j=0;j<meshes.at(i).indices.size();j++){
                   myfile2 << meshes.at(i).indices.at(j) << " ";
                   db++;
                   if(db%3==0){
                       myfile2<<endl;
                   }
               }

           }
           myfile2.close();*/

/*cout <<"all position vertices: "<< allPositionVertices.size() << endl;
cout <<"indices: "<< indicesPerFaces.size() << "\n" << endl;*/