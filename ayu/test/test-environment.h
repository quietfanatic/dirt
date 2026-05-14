#pragma once
#include <memory>
#include "../../iri/path.h"
#include "../../tap/tap.h"
#include "../resources/scheme.h"

namespace ayu::test {
    struct TestResourceScheme : FolderResourceScheme {
        using FolderResourceScheme::FolderResourceScheme;
        bool accepts_type (Type type) override {
            return type == Type::of<Collection>()
                || type == Type::of<i32>();
        }
    };
    struct TestEnvironment {
        std::unique_ptr<TestResourceScheme> trs;
        TestEnvironment () {
            auto testdir = IRI("res/dirt/ayu/test", iri::program_location());
            require(testdir);
            trs = std::make_unique<test::TestResourceScheme>(
                "ayu-test", iri::to_filepath(testdir)
            );
        }
    };
}
