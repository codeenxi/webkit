/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WasmModule.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmLLIntPlan.h"
#include "WasmModuleInformation.h"
#include "WasmWorklist.h"

namespace JSC { namespace Wasm {

Module::Module(Ref<ModuleInformation>&& moduleInformation)
    : m_moduleInformation(WTFMove(moduleInformation))
{
}

Module::~Module() { }

Wasm::SignatureIndex Module::signatureIndexFromFunctionIndexSpace(unsigned functionIndexSpace) const
{
    return m_moduleInformation->signatureIndexFromFunctionIndexSpace(functionIndexSpace);
}

static Module::ValidationResult makeValidationResult(EntryPlan& plan)
{
    ASSERT(!plan.hasWork());
    if (plan.failed())
        return Unexpected<String>(plan.errorMessage());
    return Module::ValidationResult(Module::create(plan.takeModuleInformation()));
}

static Plan::CompletionTask makeValidationCallback(Module::AsyncValidationCallback&& callback)
{
    return createSharedTask<Plan::CallbackType>([callback = WTFMove(callback)] (Plan& plan) {
        ASSERT(!plan.hasWork());
        callback->run(makeValidationResult(static_cast<LLIntPlan&>(plan)));
    });
}

Module::ValidationResult Module::validateSync(Context* context, Vector<uint8_t>&& source)
{
    Ref<LLIntPlan> plan = adoptRef(*new LLIntPlan(context, WTFMove(source), EntryPlan::Validation, Plan::dontFinalize(), nullptr, nullptr));
    plan->parseAndValidateModule();
    return makeValidationResult(plan.get());
}

void Module::validateAsync(Context* context, Vector<uint8_t>&& source, Module::AsyncValidationCallback&& callback)
{
    Ref<Plan> plan = adoptRef(*new LLIntPlan(context, WTFMove(source), EntryPlan::Validation, makeValidationCallback(WTFMove(callback)), nullptr, nullptr));
    Wasm::ensureWorklist().enqueue(WTFMove(plan));
}

Ref<CodeBlock> Module::getOrCreateCodeBlock(Context* context, MemoryMode mode, CreateEmbedderWrapper&& createEmbedderWrapper, ThrowWasmException throwWasmException)
{
    RefPtr<CodeBlock> codeBlock;
    auto locker = holdLock(m_lock);
    codeBlock = m_codeBlocks[static_cast<uint8_t>(mode)];
    // If a previous attempt at a compile errored out, let's try again.
    // Compilations from valid modules can fail because OOM and cancellation.
    // It's worth retrying.
    // FIXME: We might want to back off retrying at some point:
    // https://bugs.webkit.org/show_bug.cgi?id=170607
    if (!codeBlock || (codeBlock->compilationFinished() && !codeBlock->runnable())) {
        codeBlock = CodeBlock::create(context, mode, const_cast<ModuleInformation&>(moduleInformation()), WTFMove(createEmbedderWrapper), throwWasmException);
        m_codeBlocks[static_cast<uint8_t>(mode)] = codeBlock;
    }
    return codeBlock.releaseNonNull();
}

Ref<CodeBlock> Module::compileSync(Context* context, MemoryMode mode, CreateEmbedderWrapper&& createEmbedderWrapper, ThrowWasmException throwWasmException)
{
    Ref<CodeBlock> codeBlock = getOrCreateCodeBlock(context, mode, WTFMove(createEmbedderWrapper), throwWasmException);
    codeBlock->waitUntilFinished();
    return codeBlock;
}

void Module::compileAsync(Context* context, MemoryMode mode, CodeBlock::AsyncCompilationCallback&& task, CreateEmbedderWrapper&& createEmbedderWrapper, ThrowWasmException throwWasmException)
{
    Ref<CodeBlock> codeBlock = getOrCreateCodeBlock(context, mode, WTFMove(createEmbedderWrapper), throwWasmException);
    codeBlock->compileAsync(context, WTFMove(task));
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
