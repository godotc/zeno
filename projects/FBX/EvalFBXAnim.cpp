#include <zeno/zeno.h>
#include <zeno/utils/logger.h>
#include <zeno/extra/GlobalState.h>
#include <zeno/types/NumericObject.h>
#include <zeno/types/PrimitiveObject.h>
#include <zeno/types/DictObject.h>

#include "assimp/scene.h"

#include "Definition.h"

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

struct EvalAnim{
    double m_Duration;
    double m_TicksPerSecond;
    float m_CurrentFrame;
    float m_DeltaTime;

    NodeTree m_RootNode;

    std::unordered_map<std::string, aiMatrix4x4> m_Transforms;

    std::unordered_map<std::string, SBoneOffset> m_BoneOffset;
    std::unordered_map<std::string, SAnimBone> m_AnimBones;
    std::vector<SVertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    void initAnim(std::shared_ptr<NodeTree>& nodeTree,
                  std::shared_ptr<BoneTree>& boneTree,
                  std::shared_ptr<FBXData>& fbxData,
                  std::shared_ptr<AnimInfo>& animInfo){
        m_Duration = animInfo->duration;
        m_TicksPerSecond = animInfo->tick;

        m_Vertices = fbxData->iVertices.value;
        m_Indices = fbxData->iIndices.value;

        m_RootNode = *nodeTree;
        m_AnimBones = boneTree->AnimBoneMap;
        m_BoneOffset = fbxData->iBoneOffset.value;

        m_CurrentFrame = 0.0f;
    }

    void updateAnimation(float dt, std::shared_ptr<zeno::PrimitiveObject>& prim) {
        m_DeltaTime = dt;
        m_CurrentFrame += m_TicksPerSecond * dt;
        m_CurrentFrame = fmod(m_CurrentFrame, m_Duration);

        //zeno::log_info("Update: Frame {}", m_CurrentFrame);

        calculateBoneTransform(&m_RootNode, aiMatrix4x4());
        calculateFinal(prim);
    }

    void decomposeAnimation(std::shared_ptr<zeno::DictObject> &t,
                            std::shared_ptr<zeno::DictObject> &r,
                            std::shared_ptr<zeno::DictObject> &s){

        for(auto& m:m_Transforms){
            zeno::log_info("A {}", m.first);
            aiVector3t<float> trans;
            aiQuaterniont<float> rotate;
            aiVector3t<float> scale;
            m.second.Decompose(scale, rotate, trans);
//            zeno::log_info("    T {: f} {: f} {: f}", trans.x, trans.y, trans.z);
//            zeno::log_info("    R {: f} {: f} {: f} {: f}", rotate.x, rotate.y, rotate.z, rotate.w);
//            zeno::log_info("    S {: f} {: f} {: f}", scale.x, scale.y, scale.z);

            t->lut[m.first] = zeno::vec3f(trans.x, trans.y, trans.z);
            r->lut[m.first] = zeno::vec4f(rotate.x, rotate.y, rotate.z, rotate.w);
            s->lut[m.first] = zeno::vec3f(scale.x, scale.y, scale.z);
        }
    }

    void calculateBoneTransform(const NodeTree *node, aiMatrix4x4 parentTransform) {
        std::string nodeName = node->name;
        aiMatrix4x4 nodeTransform = node->transformation;

        if (m_AnimBones.find(nodeName) != m_AnimBones.end()) {
            auto& bone = m_AnimBones[nodeName];

            bone.update(m_CurrentFrame);
            nodeTransform = bone.m_LocalTransform;

//            zeno::log_info("+++++ {}", nodeName);
//            Helper::printAiMatrix(nodeTransform);
        }
        aiMatrix4x4 globalTransformation = parentTransform * nodeTransform;

        if (m_BoneOffset.find(nodeName) != m_BoneOffset.end()) {  // found
            std::string boneName = m_BoneOffset[nodeName].name;
            aiMatrix4x4 boneOffset = m_BoneOffset[nodeName].offset;
//            zeno::log_info("----- {}", boneName);
//            Helper::printAiMatrix(boneOffset);

            m_Transforms[boneName] = globalTransformation * boneOffset;
        }
        for (int i = 0; i < node->childrenCount; i++)
            calculateBoneTransform(&node->children[i], globalTransformation);
    }

    void calculateFinal(std::shared_ptr<zeno::PrimitiveObject>& prim){
        auto &ver = prim->verts;
        auto &ind = prim->tris;
        auto &uv = prim->verts.add_attr<zeno::vec3f>("uv");
        auto &norm = prim->verts.add_attr<zeno::vec3f>("nrm");

        for(unsigned int i=0; i<m_Vertices.size(); i++){
            auto& bwe = m_Vertices[i].boneWeights;
            auto& pos = m_Vertices[i].position;
            auto& uvw = m_Vertices[i].texCoord;
            auto& nor = m_Vertices[i].normal;

            glm::vec4 tpos(0.0f, 0.0f, 0.0f, 0.0f);

            bool infd = false;

            for(auto& b: bwe){

                infd = true;
                auto& tr = m_Transforms[b.first];
                glm::mat4 trans = glm::mat4(tr.a1,tr.b1,tr.c1,tr.d1,
                                            tr.a2,tr.b2,tr.c2,tr.d2,
                                            tr.a3,tr.b3,tr.c3,tr.d3,
                                            tr.a4,tr.b4,tr.c4,tr.d4);
                glm::vec4 lpos = trans * glm::vec4(pos.x, pos.y, pos.z, 1.0f);
                tpos += lpos * b.second;
            }
            if(! infd)
                tpos = glm::vec4(pos.x, pos.y, pos.z, 1.0f);

            glm::vec3 fpos = glm::vec3(tpos.x/tpos.w, tpos.y/tpos.w, tpos.z/tpos.w);

            ver.emplace_back(fpos.x, fpos.y, fpos.z);
            uv.emplace_back(uvw.x, uvw.y, uvw.z);
            norm.emplace_back(nor.x, nor.y, nor.z);
        }

        for(unsigned int i=0; i<m_Indices.size(); i+=3){
            zeno::vec3i incs(m_Indices[i],m_Indices[i+1],m_Indices[i+2]);
            ind.push_back(incs);
        }
    }
};

struct EvalFBXAnim : zeno::INode {

    virtual void apply() override {
        int frameid;
        if (has_input("frameid")) {
            frameid = get_input<zeno::NumericObject>("frameid")->get<int>();
        } else {
            frameid = zeno::state.frameid;
        }

        auto prim = std::make_shared<zeno::PrimitiveObject>();
        auto fbxData = get_input<FBXData>("data");
        auto nodeTree = get_input<NodeTree>("nodetree");
        auto boneTree = get_input<BoneTree>("bonetree");
        auto animInfo = get_input<AnimInfo>("animinfo");

        auto transDict = std::make_shared<zeno::DictObject>();
        auto quatDict = std::make_shared<zeno::DictObject>();
        auto scaleDict = std::make_shared<zeno::DictObject>();

        EvalAnim anim;
        anim.initAnim(nodeTree, boneTree, fbxData, animInfo);
        anim.updateAnimation(frameid/24.0f, prim);

        anim.decomposeAnimation(transDict, quatDict, scaleDict);

        set_output("prim", std::move(prim));
        set_output("transDict", std::move(transDict));
        set_output("quatDict", std::move(quatDict));
        set_output("scaleDict", std::move(scaleDict));
    }
};
ZENDEFNODE(EvalFBXAnim,
           {       /* inputs: */
               {
                   {"frameid"},
                   {"FBXData", "data"},
                   {"AnimInfo", "animinfo"},
                   {"NodeTree", "nodetree"},
                   {"BoneTree", "bonetree"},
               },  /* outputs: */
               {
                   "prim", "transDict", "quatDict", "scaleDict"
               },  /* params: */
               {

               },  /* category: */
               {
                   "primitive",
               }
           });
