#include <zeno/zeno.h>
#include <zeno/extra/TreeNode.h>
#include <zeno/types/TreeObject.h>
#include <zeno/types/NumericObject.h>
#include <zeno/utils/string.h>

namespace zeno {


struct TreeMakeFloat : TreeNode {
    virtual int determineType(EmissionPass *em) override {
        auto t1 = em->determineType(get_input("x").get());
        if (t1 != 1)
            throw zeno::Exception("TreeMakeFloat expect its input to be scalar");
        return 1;
    }

    virtual void emitCode(EmissionPass *em) override {
        auto t1 = em->determineExpr(get_input("x").get());

        em->emitCode("float(" + t1 + ")");
    }
};


ZENDEFNODE(TreeMakeFloat, {
    {
        {"float", "x", "0"},
    },
    {
        {"vec2f", "out"},
    },
    {},
    {"tree"},
});


struct TreeMakeVec2 : TreeNode {
    virtual int determineType(EmissionPass *em) override {
        auto t1 = em->determineType(get_input("x").get());
        auto t2 = em->determineType(get_input("y").get());
        if (t1 != 1 || t2 != 1)
            throw zeno::Exception("TreeMakeVec2 expect all input to be scalar");
        return 2;
    }

    virtual void emitCode(EmissionPass *em) override {
        auto t1 = em->determineExpr(get_input("x").get());
        auto t2 = em->determineExpr(get_input("y").get());

        em->emitCode("vec2(" + t1 + ", " + t2 + ")");
    }
};


ZENDEFNODE(TreeMakeVec4, {
    {
        {"float", "x", "0"},
        {"float", "y", "0"},
    },
    {
        {"vec2f", "out"},
    },
    {},
    {"tree"},
});


struct TreeMakeVec3 : TreeNode {
    virtual int determineType(EmissionPass *em) override {
        auto t1 = em->determineType(get_input("x").get());
        auto t2 = em->determineType(get_input("y").get());
        auto t3 = em->determineType(get_input("z").get());
        if (t1 != 1 || t2 != 1 || t3 != 1)
            throw zeno::Exception("TreeMakeVec3 expect all input to be scalar");
        return 3;
    }

    virtual void emitCode(EmissionPass *em) override {
        auto t1 = em->determineExpr(get_input("x").get());
        auto t2 = em->determineExpr(get_input("y").get());
        auto t3 = em->determineExpr(get_input("z").get());

        em->emitCode("vec3(" + t1 + ", " + t2 + ", " + t3 + ")");
    }
};


ZENDEFNODE(TreeMakeVec3, {
    {
        {"float", "x", "0"},
        {"float", "y", "0"},
        {"float", "z", "0"},
    },
    {
        {"vec3f", "out"},
    },
    {},
    {"tree"},
});


struct TreeMakeVec4 : TreeNode {
    virtual int determineType(EmissionPass *em) override {
        auto t1 = em->determineType(get_input("x").get());
        auto t2 = em->determineType(get_input("y").get());
        auto t3 = em->determineType(get_input("z").get());
        auto t4 = em->determineType(get_input("w").get());
        if (t1 != 1 || t2 != 1 || t3 != 1 || t4 != 1)
            throw zeno::Exception("TreeMakeVec4 expect all input to be scalar");
        return 4;
    }

    virtual void emitCode(EmissionPass *em) override {
        auto t1 = em->determineExpr(get_input("x").get());
        auto t2 = em->determineExpr(get_input("y").get());
        auto t3 = em->determineExpr(get_input("z").get());
        auto t4 = em->determineExpr(get_input("w").get());

        em->emitCode("vec4(" + t1 + ", " + t2 + ", " + t3 + ", " + t4 + ")");
    }
};


ZENDEFNODE(TreeMakeVec4, {
    {
        {"float", "x", "0"},
        {"float", "y", "0"},
        {"float", "z", "0"},
        {"float", "w", "0"},
    },
    {
        {"vec4f", "out"},
    },
    {},
    {"tree"},
});


std::shared_ptr<TreeObject> treeMakeVec(std::shared_ptr<IObject> x) {
    auto node = std::make_shared<TreeMakeFloat>();
    node->inputs["x"] = x;
    node->apply();
    auto out = safe_any_cast<std::shared_ptr<TreeObject>>(node->outputs.at("out"));
    out->extra_data = node;
    out->node = node.get();
    return out;
}


std::shared_ptr<TreeObject> treeMakeVec(std::shared_ptr<IObject> x, std::shared_ptr<IObject> y) {
    auto node = std::make_shared<TreeMakeVec2>();
    node->inputs["x"] = x;
    node->inputs["y"] = y;
    node->apply();
    auto out = safe_any_cast<std::shared_ptr<TreeObject>>(node->outputs.at("out"));
    out->extra_data = node;
    out->node = node.get();
    return out;
}


std::shared_ptr<TreeObject> treeMakeVec(std::shared_ptr<IObject> x, std::shared_ptr<IObject> y, std::shared_ptr<IObject> z) {
    auto node = std::make_shared<TreeMakeVec3>();
    node->inputs["x"] = x;
    node->inputs["y"] = y;
    node->inputs["z"] = z;
    node->apply();
    auto out = safe_any_cast<std::shared_ptr<TreeObject>>(node->outputs.at("out"));
    out->extra_data = node;
    out->node = node.get();
    return out;
}


std::shared_ptr<TreeObject> treeMakeVec(std::shared_ptr<IObject> x, std::shared_ptr<IObject> y, std::shared_ptr<IObject> z, std::shared_ptr<IObject> w) {
    auto node = std::make_shared<TreeMakeVec4>();
    node->inputs["x"] = x;
    node->inputs["y"] = y;
    node->inputs["z"] = z;
    node->inputs["w"] = w;
    node->apply();
    auto out = safe_any_cast<std::shared_ptr<TreeObject>>(node->outputs.at("out"));
    out->extra_data = node;
    out->node = node.get();
    return out;
}


std::shared_ptr<IObject> treeNum(NumericValue value) {
    return std::make_shared<NumericObject>(value);
}


}