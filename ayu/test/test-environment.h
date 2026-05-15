#pragma once
#include <memory>
#include "../../iri/path.h"
#include "../../tap/tap.h"
#include "../resources/scheme.h"

namespace ayu::test {
    struct TestScheme : FolderScheme {
        using FolderScheme::FolderScheme;
        bool accepts_type (Type type) override {
            return type == Type::of<Collection>()
                || type == Type::of<i32>();
        }
        bool allows_save (const IRI& name) override {
            return name == "ayu-test:/test-output.ayu";
        }
    };
    struct TestEnvironment {
        std::unique_ptr<TestScheme> trs;
        TestEnvironment () {
            auto testdir = IRI("res/dirt/ayu/test", iri::program_location());
            require(testdir);
            trs = std::make_unique<TestScheme>(
                "ayu-test", iri::to_filepath(testdir)
            );
        }
    };
}
