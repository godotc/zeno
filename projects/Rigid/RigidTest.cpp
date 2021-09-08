#include <zeno/zeno.h>
#include <zeno/ListObject.h>
#include <zeno/NumericObject.h>
#include <zeno/PrimitiveObject.h>
#include <zeno/utils/UserData.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <LinearMath/btConvexHullComputer.h>
#include <hacdCircularList.h>
#include <spdlog/spdlog.h>
#include <hacdVector.h>
#include <hacdICHull.h>
#include <hacdGraph.h>
#include <hacdHACD.h>
#include <memory>
#include <vector>

namespace {
using namespace zeno;


struct BulletTransform : zeno::IObject {
    btTransform trans;
};

struct BulletCollisionShape : zeno::IObject {
    std::unique_ptr<btCollisionShape> shape;

    BulletCollisionShape(std::unique_ptr<btCollisionShape> &&shape)
        : shape(std::move(shape)) {
    }
};

struct BulletCompoundShape : BulletCollisionShape {
    std::vector<std::shared_ptr<BulletCollisionShape>> children;

    using BulletCollisionShape::BulletCollisionShape;

    void addChild(btTransform const &trans,
        std::shared_ptr<BulletCollisionShape> child) {
        auto comShape = static_cast<btCompoundShape *>(shape.get());
        comShape->addChildShape(trans, child->shape.get());
        children.push_back(std::move(child));
    }
};

struct BulletMakeBoxShape : zeno::INode {
    virtual void apply() override {
        auto size = get_input<zeno::NumericObject>("semiSize")->get<zeno::vec3f>();
        auto shape = std::make_shared<BulletCollisionShape>(
            std::make_unique<btBoxShape>(zeno::vec_to_other<btVector3>(size)));
        set_output("shape", std::move(shape));
    }
};

ZENDEFNODE(BulletMakeBoxShape, {
    {{"vec3f", "semiSize", "1,1,1"}},
    {"shape"},
    {},
    {"Rigid"},
});

struct BulletMakeSphereShape : zeno::INode {
    virtual void apply() override {
        auto radius = get_input<zeno::NumericObject>("radius")->get<float>();
        auto shape = std::make_unique<BulletCollisionShape>(
            std::make_unique<btSphereShape>(btScalar(radius)));
        set_output("shape", std::move(shape));
    }
};

ZENDEFNODE(BulletMakeSphereShape, {
    {{"float", "radius", "1"}},
    {"shape"},
    {},
    {"Rigid"},
});


struct BulletTriangleMesh : zeno::IObject {
    btTriangleMesh mesh;
};

struct PrimitiveToBulletMesh : zeno::INode {
    virtual void apply() override {
        auto prim = get_input<zeno::PrimitiveObject>("prim");
        auto mesh = std::make_unique<BulletTriangleMesh>();
        auto pos = prim->attr<zeno::vec3f>("pos");
        for (int i = 0; i < prim->tris.size(); i++) {
            auto f = prim->tris[i];
            mesh->mesh.addTriangle(
                zeno::vec_to_other<btVector3>(pos[f[0]]),
                zeno::vec_to_other<btVector3>(pos[f[1]]),
                zeno::vec_to_other<btVector3>(pos[f[2]]), true);
        }
        set_output("mesh", std::move(mesh));
    }
};

ZENDEFNODE(PrimitiveToBulletMesh, {
    {"prim"},
    {"mesh"},
    {},
    {"Rigid"},
});

struct PrimitiveConvexDecomposition : zeno::INode {
    virtual void apply() override {
        auto prim = get_input<zeno::PrimitiveObject>("prim");
        auto &pos = prim->attr<zeno::vec3f>("pos");

        std::vector<HACD::Vec3<HACD::Real>> points;
        std::vector<HACD::Vec3<long>> triangles;

        for (int i = 0; i < pos.size(); i++) {
            points.push_back(
                zeno::vec_to_other<HACD::Vec3<HACD::Real>>(pos[i]));
        }

        for (int i = 0; i < prim->tris.size(); i++) {
            triangles.push_back(
                zeno::vec_to_other<HACD::Vec3<long>>(prim->tris[i]));
        }

        HACD::HACD hacd;
        hacd.SetPoints(points.data());
        hacd.SetNPoints(points.size());
        hacd.SetTriangles(triangles.data());
        hacd.SetNTriangles(triangles.size());

		hacd.SetCompacityWeight(0.1);
		hacd.SetVolumeWeight(0.0);
		hacd.SetNClusters(2);
		hacd.SetNVerticesPerCH(100);
		hacd.SetConcavity(100.0);
		hacd.SetAddExtraDistPoints(false);
		hacd.SetAddNeighboursDistPoints(false);
		hacd.SetAddFacesPoints(false);

        hacd.Compute();
        size_t nClusters = hacd.GetNClusters();

        auto listPrim = std::make_shared<zeno::ListObject>();
        listPrim->arr.clear();

        printf("hacd got %d clusters\n", nClusters);
        for (size_t c = 0; c < nClusters; c++) {
            size_t nPoints = hacd.GetNPointsCH(c);
            size_t nTriangles = hacd.GetNTrianglesCH(c);
            printf("hacd cluster %d have %d points, %d triangles\n",
                c, nPoints, nTriangles);

            points.clear();
            points.resize(nPoints);
            triangles.clear();
            triangles.resize(nTriangles);
            hacd.GetCH(c, points.data(), triangles.data());

            auto outprim = std::make_shared<zeno::PrimitiveObject>();
            outprim->resize(nPoints);
            outprim->tris.resize(nTriangles);

            auto &outpos = outprim->add_attr<zeno::vec3f>("pos");
            for (size_t i = 0; i < nPoints; i++) {
                auto p = points[i];
                //printf("point %d: %f %f %f\n", i, p.X(), p.Y(), p.Z());
                outpos[i] = zeno::vec3f(p.X(), p.Y(), p.Z());
            }

            for (size_t i = 0; i < nTriangles; i++) {
                auto p = triangles[i];
                //printf("triangle %d: %d %d %d\n", i, p.X(), p.Y(), p.Z());
                outprim->tris[i] = zeno::vec3i(p.X(), p.Y(), p.Z());
            }

            listPrim->arr.push_back(std::move(outprim));
        }

        set_output("listPrim", std::move(listPrim));
    }
};

ZENDEFNODE(PrimitiveConvexDecomposition, {
    {"prim"},
    {"listPrim"},
    {},
    {"Rigid"},
});


struct BulletMakeConvexMeshShape : zeno::INode {
    virtual void apply() override {
        auto triMesh = &get_input<BulletTriangleMesh>("triMesh")->mesh;
        auto inShape = std::make_unique<btConvexTriangleMeshShape>(triMesh);

        auto shape = std::make_shared<BulletCollisionShape>(std::move(inShape));
        set_output("shape", std::move(shape));
    }
};

ZENDEFNODE(BulletMakeConvexMeshShape, {
    {"triMesh"},
    {"shape"},
    {},
    {"Rigid"},
});

struct BulletMakeConvexHullShape : zeno::INode {
    virtual void apply() override {
        auto triMesh = &get_input<BulletTriangleMesh>("triMesh")->mesh;

#if 1
        auto inShape = std::make_unique<btConvexTriangleMeshShape>(triMesh);
        auto hull = std::make_unique<btShapeHull>(inShape.get());
        auto margin = get_input2<float>("margin");
        auto highres = get_input2<int>("highres");
        hull->buildHull(margin, highres);
        auto convex = std::make_unique<btConvexHullShape>(
             (const btScalar *)hull->getVertexPointer(), hull->numVertices());
        convex->setMargin(btScalar(margin));
#else
        auto convexHC = std::make_unique<btConvexHullComputer>();
        std::vector<float> vertices;
        for (int i = 0; i < inShape->getNumVertices(); i++) {
            btVector3 coor;
            //inShape->btTriangleIndexVertexArray::preallocateVertices
            inShape->getVertex(i, coor);
            vertices.push_back(coor[0]);
            vertices.push_back(coor[1]);
            vertices.push_back(coor[2]);
        }
        convexHC->compute(vertices.data(), sizeof(float) * 3, vertices.size() / 3, 0.04f, 0.0f);
        auto convex = std::make_unique<btConvexHullShape>(&(convexHC->vertices[0].getX()), convexHC->vertices.size());
#endif

        // auto convex = std::make_unique<btConvexPointCloudShape>();
        // btVector3* points = new btVector3[inShape->getNumVertices()];
        // for(int i=0;i<inShape->getNumVertices(); i++)
        // {
        //     btVector3 v;
        //     inShape->getVertex(i, v);
        //     points[i]=v;
        // }

        auto shape = std::make_shared<BulletCollisionShape>(std::move(convex));
        set_output("shape", std::move(shape));
    }
};

ZENDEFNODE(BulletMakeConvexHullShape, {
    {"triMesh", {"float", "margin", "0"}, {"int", "highres", "0"}},
    {"shape"},
    {},
    {"Rigid"},
});

struct BulletMakeCompoundShape : zeno::INode {
    virtual void apply() override {
        auto compound = std::make_unique<btCompoundShape>();
        auto shape = std::make_shared<BulletCompoundShape>(std::move(compound));
        set_output("compound", std::move(shape));
    }
};

ZENDEFNODE(BulletMakeCompoundShape, {
    {""},
    {"compound"},
    {},
    {"Rigid"},
});

struct BulletCompoundAddChild : zeno::INode {
    virtual void apply() override {
        auto compound = get_input<BulletCompoundShape>("compound");
        auto childShape = get_input<BulletCollisionShape>("childShape");
        auto trans = get_input<BulletTransform>("trans")->trans;

        compound->addChild(trans, std::move(childShape));
        set_output("compound", get_input("compound"));
    }
};

ZENDEFNODE(BulletCompoundAddChild, {
    {"compound", "childShape", "trans"},
    {"compound"},
    {},
    {"Rigid"},
});


struct BulletMakeTransform : zeno::INode {
    virtual void apply() override {
        auto trans = std::make_unique<BulletTransform>();
        trans->trans.setIdentity();
        if (has_input("origin")) {
            auto origin = get_input<zeno::NumericObject>("origin")->get<zeno::vec3f>();
            trans->trans.setOrigin(zeno::vec_to_other<btVector3>(origin));
        }
        if (has_input("rotation")) {
            if (get_input<zeno::NumericObject>("rotation")->is<zeno::vec3f>()) {
                auto rotation = get_input<zeno::NumericObject>("rotation")->get<zeno::vec3f>();
                trans->trans.setRotation(zeno::vec_to_other<btQuaternion>(rotation));
            } else {
                auto rotation = get_input<zeno::NumericObject>("rotation")->get<zeno::vec4f>();
                trans->trans.setRotation(zeno::vec_to_other<btQuaternion>(rotation));
            }
        }
        set_output("trans", std::move(trans));
    }
};

ZENDEFNODE(BulletMakeTransform, {
    {{"vec3f", "origin"}, {"vec3f", "rotation"}},
    {"trans"},
    {},
    {"Rigid"},
});

struct BulletComposeTransform : zeno::INode {
    virtual void apply() override {
        auto transFirst = get_input<BulletTransform>("transFirst")->trans;
        auto transSecond = get_input<BulletTransform>("transSecond")->trans;
        auto trans = std::make_unique<BulletTransform>();
        trans->trans = transFirst * transSecond;
        set_output("trans", std::move(trans));
    }
};

ZENDEFNODE(BulletComposeTransform, {
    {"transFirst", "transSecond"},
    {"trans"},
    {},
    {"Rigid"},
});


struct BulletObject : zeno::IObject {
    std::unique_ptr<btDefaultMotionState> myMotionState;
    std::unique_ptr<btRigidBody> body;
    std::shared_ptr<BulletCollisionShape> colShape;
    btScalar mass = 0.f;
    btTransform trans;

    BulletObject(btScalar mass_,
        btTransform const &trans,
        std::shared_ptr<BulletCollisionShape> colShape_)
        : mass(mass_), colShape(std::move(colShape_))
    {
        btVector3 localInertia(0, 0, 0);
        if (mass != 0)
            colShape->shape->calculateLocalInertia(mass, localInertia);
        
        myMotionState = std::make_unique<btDefaultMotionState>(trans);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, myMotionState.get(), colShape->shape.get(), localInertia);
        body = std::make_unique<btRigidBody>(rbInfo);
    }
};

struct BulletMakeObject : zeno::INode {
    virtual void apply() override {
        auto shape = get_input<BulletCollisionShape>("shape");
        auto mass = get_input<zeno::NumericObject>("mass")->get<float>();
        auto trans = get_input<BulletTransform>("trans");
        auto object = std::make_unique<BulletObject>(
            mass, trans->trans, shape);
        object->body->setDamping(0, 0);
        object->userData = shape->userData; // todo: use a node to perform this??
        set_output("object", std::move(object));
    }
};

ZENDEFNODE(BulletMakeObject, {
    {"shape", "trans", {"float", "mass", "0"}},
    {"object"},
    {},
    {"Rigid"},
});

struct BulletGetObjectUserData : zeno::INode {
    virtual void apply() override {
        auto object = get_input<BulletObject>("object");
        auto key = get_param<std::string>("key");
        auto hasValue = object->userData.has(key);
        auto data = object->userData.get<Any>(key);
        set_output2("hasValue", hasValue);
        set_output2("data", std::move(data));
    }
};

ZENDEFNODE(BulletGetObjectUserData, {
    {"object"},
    {"data", {"bool", "hasValue"}},
    {{"string", "key", "prim"}},
    {"Rigid"},
});

struct BulletSetObjectDamping : zeno::INode {
    virtual void apply() override {
        auto object = get_input<BulletObject>("object");
        auto dampLin = get_input2<float>("dampLin");
        auto dampAug = get_input2<float>("dampAug");
        object->body->setDamping(dampLin, dampAug);
        set_output("object", std::move(object));
    }
};

ZENDEFNODE(BulletSetObjectDamping, {
    {"object", {"float", "dampLin", "0"}, {"float", "dampAug", "0"}},
    {"object"},
    {},
    {"Rigid"},
});

struct BulletSetObjectFriction : zeno::INode {
    virtual void apply() override {
        auto object = get_input<BulletObject>("object");
        auto friction = get_input2<float>("friction");
        object->body->setFriction(friction);
        set_output("object", std::move(object));
    }
};

ZENDEFNODE(BulletSetObjectFriction, {
    {"object", {"float", "friction", "0"}},
    {"object"},
    {},
    {"Rigid"},
});

struct BulletSetObjectRestitution : zeno::INode {
    virtual void apply() override {
        auto object = get_input<BulletObject>("object");
        auto restitution = get_input2<float>("restitution");
        object->body->setRestitution(restitution);
        set_output("object", std::move(object));
    }
};

ZENDEFNODE(BulletSetObjectRestitution, {
    {"object", {"float", "restitution", "0"}},
    {"object"},
    {},
    {"Rigid"},
});

struct BulletGetObjTransform : zeno::INode {
    virtual void apply() override {
        auto obj = get_input<BulletObject>("object");
        auto body = obj->body.get();
        auto trans = std::make_unique<BulletTransform>();
        if (body && body->getMotionState()) {
            body->getMotionState()->getWorldTransform(trans->trans);
        } else {
            trans->trans = static_cast<btCollisionObject *>(body)->getWorldTransform();
        }
        set_output("trans", std::move(trans));
    }
};

ZENDEFNODE(BulletGetObjTransform, {
    {"object"},
    {"trans"},
    {},
    {"Rigid"},
});

struct BulletGetObjMotion : zeno::INode {
    virtual void apply() override {
        auto obj = get_input<BulletObject>("object");
        auto body = obj->body.get();
        auto linearVel = zeno::IObject::make<zeno::NumericObject>();
        auto angularVel = zeno::IObject::make<zeno::NumericObject>();
        linearVel->set<zeno::vec3f>(zeno::vec3f(0));
        angularVel->set<zeno::vec3f>(zeno::vec3f(0));

        if (body && body->getLinearVelocity() ) {
            auto v = body->getLinearVelocity();
            linearVel->set<zeno::vec3f>(zeno::vec3f(v.x(), v.y(), v.z()));
        }
        if (body && body->getAngularVelocity() ){
            auto w = body->getAngularVelocity();
            angularVel->set<zeno::vec3f>(zeno::vec3f(w.x(), w.y(), w.z()));
        }
        set_output("linearVel", linearVel);
        set_output("angularVel", angularVel);
    }
};

ZENDEFNODE(BulletGetObjMotion, {
    {"object"},
    {"linearVel", "angularVel"},
    {},
    {"Rigid"},
});


struct BulletConstraint : zeno::IObject {
    std::unique_ptr<btTypedConstraint> constraint;

    BulletObject *obj1;
    BulletObject *obj2;

    BulletConstraint(BulletObject *obj1, BulletObject *obj2)
        : obj1(obj1), obj2(obj2)
    {
        //btTransform gf;
        //gf.setIdentity();
        //gf.setOrigin(cposw);
        auto trA = obj1->body->getWorldTransform().inverse();// * gf;
        auto trB = obj2->body->getWorldTransform().inverse();// * gf;
#if 0
        constraint = std::make_unique<btFixedConstraint>(
                *obj1->body, *obj2->body, trA, trB);
#else
        constraint = std::make_unique<btGeneric6DofConstraint>(
                *obj1->body, *obj2->body, trA, trB, true);
        for (int i = 0; i < 6; i++)
            static_cast<btGeneric6DofConstraint *>(constraint.get())->setLimit(i, 0, 0);
#endif
    }

    void setBreakingThreshold(float breakingThreshold) {
        auto totalMass = obj1->body->getMass() + obj2->body->getMass();
        constraint->setBreakingImpulseThreshold(breakingThreshold * totalMass);
    }
};

struct BulletMakeConstraint : zeno::INode {
    virtual void apply() override {
        auto obj1 = get_input<BulletObject>("obj1");
        auto obj2 = get_input<BulletObject>("obj2");
        auto cons = std::make_shared<BulletConstraint>(obj1.get(), obj2.get());
        //cons->constraint->setOverrideNumSolverIterations(400);
        set_output("constraint", std::move(cons));
    }
};

ZENDEFNODE(BulletMakeConstraint, {
    {"obj1", "obj2"},
    {"constraint"},
    {},
    {"Rigid"},
});

struct BulletSetConstraintBreakThres : zeno::INode {
    virtual void apply() override {
        auto cons = get_input<BulletConstraint>("constraint");
        cons->setBreakingThreshold(get_input2<float>("threshold"));
        set_output("constraint", std::move(cons));
    }
};

ZENDEFNODE(BulletSetConstraintBreakThres, {
    {"constraint", {"float", "threshold", "3.0"}},
    {"constraint"},
    {},
    {"Rigid"},
});


struct RigidVelToPrimitive : zeno::INode {
    virtual void apply() override {
        auto prim = get_input<zeno::PrimitiveObject>("prim");
        auto com = get_input<zeno::NumericObject>("centroid")->get<zeno::vec3f>();
        auto lin = get_input<zeno::NumericObject>("linearVel")->get<zeno::vec3f>();
        auto ang = get_input<zeno::NumericObject>("angularVel")->get<zeno::vec3f>();

        auto &pos = prim->attr<zeno::vec3f>("pos");
        auto &vel = prim->add_attr<zeno::vec3f>("vel");
        #pragma omp parallel for
        for (size_t i = 0; i < prim->size(); i++) {
            vel[i] = lin + zeno::cross(ang, pos[i] - com);
        }

        set_output("prim", get_input("prim"));
    }
};

ZENDEFNODE(RigidVelToPrimitive, {
    {"prim", "centroid", "linearVel", "angularVel"},
    {"prim"},
    {},
    {"Rigid"},
});

struct BulletExtractTransform : zeno::INode {
    virtual void apply() override {
        auto trans = &get_input<BulletTransform>("trans")->trans;
        auto origin = std::make_unique<zeno::NumericObject>();
        auto rotation = std::make_unique<zeno::NumericObject>();
        origin->set(vec3f(other_to_vec<3>(trans->getOrigin())));
        rotation->set(vec4f(other_to_vec<4>(trans->getRotation())));
        set_output("origin", std::move(origin));
        set_output("rotation", std::move(rotation));
    }
};

ZENDEFNODE(BulletExtractTransform, {
    {"trans"},
    {{"vec3f","origin"}, {"vec4f", "rotation"}},
    {},
    {"Rigid"},
});


struct BulletWorld : zeno::IObject {
    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    std::unique_ptr<btCollisionDispatcher> dispatcher = std::make_unique<btCollisionDispatcher>(collisionConfiguration.get());
    std::unique_ptr<btBroadphaseInterface> overlappingPairCache = std::make_unique<btDbvtBroadphase>();
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver = std::make_unique<btSequentialImpulseConstraintSolver>();

    std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), overlappingPairCache.get(), solver.get(), collisionConfiguration.get());

    std::set<std::shared_ptr<BulletObject>> objects;
    std::set<std::shared_ptr<BulletConstraint>> constraints;

    BulletWorld() {
        dynamicsWorld->setGravity(btVector3(0, -10, 0));
    }

    void addObject(std::shared_ptr<BulletObject> obj) {
        spdlog::trace("adding object {}", (void *)obj.get());
        dynamicsWorld->addRigidBody(obj->body.get());
        objects.insert(std::move(obj));
    }

    void removeObject(std::shared_ptr<BulletObject> const &obj) {
        spdlog::trace("removing object {}", (void *)obj.get());
        dynamicsWorld->removeRigidBody(obj->body.get());
        objects.erase(obj);
    }

    void setObjectList(std::vector<std::shared_ptr<BulletObject>> objList) {
        std::set<std::shared_ptr<BulletObject>> objSet;
        spdlog::trace("setting object list len={}", objList.size());
        spdlog::trace("existing object list len={}", objects.size());
        for (auto const &object: objList) {
            objSet.insert(object);
            if (objects.find(object) == objects.end()) {
                addObject(std::move(object));
            }
        }
        for (auto const &object: std::set(objects)) {
            if (objSet.find(object) == objSet.end()) {
                removeObject(object);
            }
        }
    }

    void addConstraint(std::shared_ptr<BulletConstraint> cons) {
        spdlog::trace("adding constraint {}", (void *)cons.get());
        dynamicsWorld->addConstraint(cons->constraint.get(), true);
        constraints.insert(std::move(cons));
    }

    void removeConstraint(std::shared_ptr<BulletConstraint> const &cons) {
        spdlog::info("removing constraint {}", (void *)cons.get());
        dynamicsWorld->removeConstraint(cons->constraint.get());
        constraints.erase(cons);
    }

    void setConstraintList(std::vector<std::shared_ptr<BulletConstraint>> consList) {
        std::set<std::shared_ptr<BulletConstraint>> consSet;
        spdlog::trace("setting constraint list len={}", consList.size());
        spdlog::trace("existing constraint list len={}", constraints.size());
        for (auto const &constraint: consList) {
            if (!constraint->constraint->isEnabled())
                continue;
            consSet.insert(constraint);
            if (constraints.find(constraint) == constraints.end()) {
                addConstraint(std::move(constraint));
            }
        }
        for (auto const &constraint: std::set(constraints)) {
            if (consSet.find(constraint) == consSet.end()) {
                removeConstraint(constraint);
            }
        }
    }

    /*
    void addGround() {
        auto groundShape = std::make_unique<btBoxShape>(btVector3(btScalar(50.), btScalar(50.), btScalar(50.)));

        btTransform groundTransform;
        groundTransform.setIdentity();
        groundTransform.setOrigin(btVector3(0, -56, 0));

        btScalar mass(0.);

        addObject(std::make_unique<BulletObject>(mass, groundTransform, std::move(groundShape)));
    }

    void addBall() {
        auto colShape = std::make_unique<btSphereShape>(btScalar(1.));

        btTransform startTransform;
        startTransform.setIdentity();

        btScalar mass(1.f);

        addObject(std::make_unique<BulletObject>(mass, startTransform, std::move(colShape)));
    }*/

    void step(float dt = 1.f / 60.f, int steps = 1) {
        spdlog::info("stepping with dt={}, steps={}, len(objects)={}", dt, steps, objects.size());
        //dt /= steps;
        //for(int i=0;i<steps;i++)
        dynamicsWorld->stepSimulation(dt, steps, dt / steps);

        /*for (int j = dynamicsWorld->getNumCollisionObjects() - 1; j >= 0; j--)
        {
            btCollisionObject* obj = dynamicsWorld->getCollisionObjectArray()[j];
            btRigidBody* body = btRigidBody::upcast(obj);
            btTransform trans;
            if (body && body->getMotionState())
            {
                body->getMotionState()->getWorldTransform(trans);
            }
            else
            {
                trans = obj->getWorldTransform();
            }
            printf("world pos object %d = %f,%f,%f\n", j, float(trans.getOrigin().getX()), float(trans.getOrigin().getY()), float(trans.getOrigin().getZ()));
        }*/
    }
};

struct BulletMakeWorld : zeno::INode {
    virtual void apply() override {
        auto world = std::make_unique<BulletWorld>();
        set_output("world", std::move(world));
    }
};

ZENDEFNODE(BulletMakeWorld, {
    {},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletSetWorldGravity : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto gravity = get_input<zeno::NumericObject>("gravity")->get<zeno::vec3f>();
        world->dynamicsWorld->setGravity(zeno::vec_to_other<btVector3>(gravity));
        set_output("world", std::move(world));
    }
};

ZENDEFNODE(BulletSetWorldGravity, {
    {"world", {"vec3f", "gravity", "0,0,-9.8"}},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletStepWorld : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto dt = get_input<zeno::NumericObject>("dt")->get<float>();
        auto steps = get_input<zeno::NumericObject>("steps")->get<int>();
        world->step(dt, steps);
        set_output("world", std::move(world));
    }
};

ZENDEFNODE(BulletStepWorld, {
    {"world", {"float", "dt", "0.04"}, {"int", "steps", "1"}},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldAddObject : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto object = get_input<BulletObject>("object");
        world->addObject(std::move(object));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldAddObject, {
    {"world", "object"},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldRemoveObject : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto object = get_input<BulletObject>("object");
        world->removeObject(std::move(object));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldRemoveObject, {
    {"world", "object"},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldSetObjList : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto objList = get_input<ListObject>("objList")->get<std::shared_ptr<BulletObject>>();
        world->setObjectList(std::move(objList));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldSetObjList, {
    {"world", "objList"},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldAddConstraint : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto constraint = get_input<BulletConstraint>("constraint");
        world->addConstraint(std::move(constraint));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldAddConstraint, {
    {"world", "constraint"},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldRemoveConstraint : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto constraint = get_input<BulletConstraint>("constraint");
        world->removeConstraint(std::move(constraint));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldRemoveConstraint, {
    {"world", "constraint"},
    {"world"},
    {},
    {"Rigid"},
});

struct BulletWorldSetConsList : zeno::INode {
    virtual void apply() override {
        auto world = get_input<BulletWorld>("world");
        auto consList = get_input<ListObject>("consList")
            ->get<std::shared_ptr<BulletConstraint>>();
        world->setConstraintList(std::move(consList));
        set_output("world", get_input("world"));
    }
};

ZENDEFNODE(BulletWorldSetConsList, {
    {"world", "consList"},
    {"world"},
    {},
    {"Rigid"},
});


struct BulletObjectApplyForce:zeno::INode {
    virtual void apply() override {
        auto object = get_input<BulletObject>("object");
        auto forceImpulse = get_input<zeno::NumericObject>("ForceImpulse")->get<zeno::vec3f>();
        auto torqueImpulse = get_input<zeno::NumericObject>("TorqueImpulse")->get<zeno::vec3f>();
        object->body->applyCentralImpulse(zeno::vec_to_other<btVector3>(forceImpulse));
        object->body->applyTorqueImpulse(zeno::vec_to_other<btVector3>(torqueImpulse));
    }
};

ZENDEFNODE(BulletObjectApplyForce, {
    {"object", {"vec3f", "ForceImpulse", "0,0,0"}, {"vec3f", "TorqueImpulse", "0,0,0"}},
    {},
    {},
    {"Rigid"},
});


}
