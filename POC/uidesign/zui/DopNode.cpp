#include "DopNode.h"
#include "DopFunctor.h"
#include "DopGraph.h"
#include "DopTable.h"


void DopNode::_apply_func() {
    ztd::Vector<DopLazy> in(inputs.size());

    for (int i = 0; i < in.size(); i++) {
        in[i] = graph->resolve_value(inputs[i].value, node_changed);
    }
    if (!node_changed)
        return;

    auto func = tab.lookup(kind);
    ztd::Vector<DopLazy> out(outputs.size());
    func(in, out);

    for (int i = 0; i < out.size(); i++) {
        outputs.at(i).result = std::move(out[i]);
    }
}


DopLazy DopNode::get_output_by_name(std::string name, bool &changed) {
    int n = -1;
    for (int i = 0; i < outputs.size(); i++) {
        if (outputs[i].name == name)
            n = i;
    }
    if (n == -1)
        throw ztd::makeException("Bad output socket name: ", name);

    _apply_func();
    if (node_changed) {
        changed = true;
        node_changed = false;
    }

    auto val = outputs[n].result;
    if (!val.has_value()) {
        throw ztd::makeException("no value returned at socket: ", name);
    }
    return val;
}


void DopNode::serialize(std::ostream &ss) const {
    ss << "DopNode[" << '\n';
    ss << "  name=" << name << '\n';
    ss << "  kind=" << kind << '\n';
    ss << "  inputs=[" << '\n';
    for (auto const &input: inputs) {
        ss << "    ";
        input.serialize(ss);
        ss << '\n';
    }
    ss << "  ]" << '\n';
    ss << "  outputs=[" << '\n';
    for (auto const &output: outputs) {
        ss << "    ";
        output.serialize(ss);
        ss << '\n';
    }
    ss << "  ]" << '\n';
    ss << "]" << '\n';
}


void DopNode::invalidate() {
    for (auto const &output: outputs) {
        output.result.reset();
    }
}