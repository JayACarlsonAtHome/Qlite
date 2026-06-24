module;

#include "Qlite/Setup/v002.hpp"

export module Qlite.setup.v002;

// Re-export the public toolchain-sensing API under Setup::v002.
export using Setup::v002::CaptureResult;
export using Setup::v002::Toolchain;
export using Setup::v002::ResolvedToolchain;
export using Setup::v002::EnvSnapshot;
export using Setup::v002::run_capture;
export using Setup::v002::load_registry;
export using Setup::v002::resolve_toolchain;
export using Setup::v002::resolve_registry;
export using Setup::v002::find_toolchain_by_label;
export using Setup::v002::find_toolchain_by_flag;
export using Setup::v002::parse_version_major;
export using Setup::v002::HostSnapshot;
export using Setup::v002::sense_host;
export using Setup::v002::ReadinessReport;
export using Setup::v002::assess_readiness;
export using Setup::v002::write_filecheck_list;
export using Setup::v002::ProvisionRecipe;
export using Setup::v002::load_recipes;
export using Setup::v002::ProvisionStep;
export using Setup::v002::ProvisionPlan;
export using Setup::v002::plan_provision;
export using Setup::v002::write_setup_script;
export using Setup::v002::scrub_exported_shell_functions;
export using Setup::v002::snapshot_env;
export using Setup::v002::apply_env_delta;
export using Setup::v002::restore_env;
