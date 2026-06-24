module;

#include "Qlite/Setup/v001.hpp"

export module Qlite.setup.v001;

// Re-export the public toolchain-sensing API under Setup::v001.
export using Setup::v001::CaptureResult;
export using Setup::v001::Toolchain;
export using Setup::v001::ResolvedToolchain;
export using Setup::v001::EnvSnapshot;
export using Setup::v001::run_capture;
export using Setup::v001::load_registry;
export using Setup::v001::resolve_toolchain;
export using Setup::v001::resolve_registry;
export using Setup::v001::find_toolchain_by_label;
export using Setup::v001::find_toolchain_by_flag;
export using Setup::v001::parse_version_major;
export using Setup::v001::HostSnapshot;
export using Setup::v001::sense_host;
export using Setup::v001::ReadinessReport;
export using Setup::v001::assess_readiness;
export using Setup::v001::write_filecheck_list;
export using Setup::v001::ProvisionRecipe;
export using Setup::v001::load_recipes;
export using Setup::v001::ProvisionStep;
export using Setup::v001::ProvisionPlan;
export using Setup::v001::plan_provision;
export using Setup::v001::write_setup_script;
export using Setup::v001::scrub_exported_shell_functions;
export using Setup::v001::snapshot_env;
export using Setup::v001::apply_env_delta;
export using Setup::v001::restore_env;
