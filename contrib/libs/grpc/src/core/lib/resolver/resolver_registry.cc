//
// Copyright 2015 gRPC authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <grpc/support/port_platform.h>

#include "src/core/lib/resolver/resolver_registry.h"

#include <string.h>

#include <vector>

#include "y_absl/memory/memory.h"
#include "y_absl/strings/str_cat.h"
#include "y_absl/strings/str_format.h"

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/string_util.h>

#include "src/core/lib/resolver/resolver_registry.h"

namespace grpc_core {

//
// ResolverRegistry::Builder
//

ResolverRegistry::Builder::Builder() { Reset(); }

void ResolverRegistry::Builder::SetDefaultPrefix(TString default_prefix) {
  state_.default_prefix = std::move(default_prefix);
}

void ResolverRegistry::Builder::RegisterResolverFactory(
    std::unique_ptr<ResolverFactory> factory) {
  auto p = state_.factories.emplace(factory->scheme(), std::move(factory));
  GPR_ASSERT(p.second);
}

bool ResolverRegistry::Builder::HasResolverFactory(
    y_absl::string_view scheme) const {
  return state_.factories.find(scheme) != state_.factories.end();
}

void ResolverRegistry::Builder::Reset() {
  state_.factories.clear();
  state_.default_prefix = "dns:///";
}

ResolverRegistry ResolverRegistry::Builder::Build() {
  return ResolverRegistry(std::move(state_));
}

//
// ResolverRegistry
//

bool ResolverRegistry::IsValidTarget(y_absl::string_view target) const {
  TString canonical_target;
  URI uri;
  ResolverFactory* factory =
      FindResolverFactory(target, &uri, &canonical_target);
  if (factory == nullptr) return false;
  return factory->IsValidUri(uri);
}

OrphanablePtr<Resolver> ResolverRegistry::CreateResolver(
    y_absl::string_view target, const grpc_channel_args* args,
    grpc_pollset_set* pollset_set,
    std::shared_ptr<WorkSerializer> work_serializer,
    std::unique_ptr<Resolver::ResultHandler> result_handler) const {
  TString canonical_target;
  ResolverArgs resolver_args;
  ResolverFactory* factory =
      FindResolverFactory(target, &resolver_args.uri, &canonical_target);
  if (factory == nullptr) return nullptr;
  resolver_args.args = args;
  resolver_args.pollset_set = pollset_set;
  resolver_args.work_serializer = std::move(work_serializer);
  resolver_args.result_handler = std::move(result_handler);
  return factory->CreateResolver(std::move(resolver_args));
}

TString ResolverRegistry::GetDefaultAuthority(
    y_absl::string_view target) const {
  TString canonical_target;
  URI uri;
  ResolverFactory* factory =
      FindResolverFactory(target, &uri, &canonical_target);
  if (factory == nullptr) return "";
  return factory->GetDefaultAuthority(uri);
}

TString ResolverRegistry::AddDefaultPrefixIfNeeded(
    y_absl::string_view target) const {
  TString canonical_target;
  URI uri;
  FindResolverFactory(target, &uri, &canonical_target);
  return canonical_target.empty() ? TString(target) : canonical_target;
}

ResolverFactory* ResolverRegistry::LookupResolverFactory(
    y_absl::string_view scheme) const {
  auto it = state_.factories.find(scheme);
  if (it == state_.factories.end()) return nullptr;
  return it->second.get();
}

// Returns the factory for the scheme of \a target.  If \a target does
// not parse as a URI, prepends \a default_prefix_ and tries again.
// If URI parsing is successful (in either attempt), sets \a uri to
// point to the parsed URI.
ResolverFactory* ResolverRegistry::FindResolverFactory(
    y_absl::string_view target, URI* uri, TString* canonical_target) const {
  GPR_ASSERT(uri != nullptr);
  y_absl::StatusOr<URI> tmp_uri = URI::Parse(target);
  ResolverFactory* factory =
      tmp_uri.ok() ? LookupResolverFactory(tmp_uri->scheme()) : nullptr;
  if (factory != nullptr) {
    *uri = std::move(*tmp_uri);
    return factory;
  }
  *canonical_target = y_absl::StrCat(state_.default_prefix, target);
  y_absl::StatusOr<URI> tmp_uri2 = URI::Parse(*canonical_target);
  factory = tmp_uri2.ok() ? LookupResolverFactory(tmp_uri2->scheme()) : nullptr;
  if (factory != nullptr) {
    *uri = std::move(*tmp_uri2);
    return factory;
  }
  if (!tmp_uri.ok() || !tmp_uri2.ok()) {
    gpr_log(GPR_ERROR, "%s",
            y_absl::StrFormat("Error parsing URI(s). '%s':%s; '%s':%s", target,
                            tmp_uri.status().ToString(), *canonical_target,
                            tmp_uri2.status().ToString())
                .c_str());
    return nullptr;
  }
  gpr_log(GPR_ERROR, "Don't know how to resolve '%s' or '%s'.",
          TString(target).c_str(), canonical_target->c_str());
  return nullptr;
}

}  // namespace grpc_core
