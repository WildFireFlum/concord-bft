import os
import sys
from pathlib import Path

TEST_GROUPS =  { 1: ["skvbc_reconfiguration", "slowdown_test"],
                 2: ["skvbc_dbsnapshot_tests", "skvbc_view_change_tests", "skvbc_time_service", "skvbc_basic_tests",
                     "skvbc_state_transfer_tests", "skvbc_commit_path_tests", "skvbc_persistence_tests",
                     "skvbc_chaotic_startup", "s3_metrics_test"],
                 3: ["skvbc_preexecution_with_result_auth_tests","skvbc_client_transaction_signing","skvbc_preexecution_tests","skvbc_batch_preexecution_tests","skvbc_ro_replica_tests","sparse_merkle_storage_db_adapter_property_test","TestThresholdBls","skvbc_backup_restore","skvbc_checkpoints","skvbc_linearizability_tests","SigManager_test","thin_replica_server_test","skvbc_pyclient_tests","skvbc_multi_sig","TestBlsBatchVerifier","kv_blockchain_db_editor_test","skvbc_block_accumulation_tests","sparse_merkle_storage_db_adapter_unit_test","skvbc_auto_view_change_tests","skvbc_consensus_batching","bft_client_api_tests","bcstatetransfer_tests","poll_based_state_client_test","native_rocksdb_client_test","skvbc_reply_tests","categorized_kv_blockchain_unit_test","resource_manager_test","skvbc_cron","block_merkle_latest_ver_cf_migration_test","cmf_cppgen_tests","s3_client_test","skvbc_publish_clients_keys","preprocessor_test","TestGroupElementSizes","replica_state_sync_test","trc_byzantine_tests","immutable_kv_category_unit_test","versioned_kv_category_unit_test","util_mt_tests","diagnostics_tests","multiIO_test","replica_state_snapshot_service_test","pruning_test","block_merkle_category_unit_test","sparse_merkle_internal_node_property_test","categorized_blocks_unit_test","cmf_pygen_tests","replica_stream_snapshot_client_tests","categorized_blockchain_unit_test","cre_test_api","TestLagrange","TestRelicSerialization","TestRelic","cc_basic_update_queue_tests","incomingMsgsStorage_test","metadataStorage_test","ViewChangeMsg_test","sparse_merkle_base_types_test","pruning_reserved_pages_client_test","pyclient_tests","ClientsManager_test","crypto_utils_test","ViewChange_tests","kvbc_filter_test","test_skvbc_history_tracker","skvbc_network_partitioning_tests","skvbc_byzantine_primary_preexecution_tests","test_serialization","openssl_crypto_wrapper_test","source_selector_test","metric_server_tests","blockchain_view_test","thin_replica_client_tests","views_manager_test","PrePrepareMsg_test","ClientPreProcessRequestMsg_test","PreProcessReplyMsg_test","PreProcessResultMsg_test","seqNumForClientRequest_test","RequestThreadPool_test","trs_sub_buffer_test","ReplicaAsksToLeaveViewMsg_test","ControllerWithSimpleHistory_test","ClientRequestMsg_test","CheckpointMsg_test","ReqMissingDataMsg_test","StartSlowCommitMsg_test","AskForCheckpointMsg_test","FullCommitProofMsg_test","ReplicaRestartReadyMsg_test","TimeServiceResPageClient_test","TimeServiceManager_test","clientservice-test-yaml_parsing","msgsCertificate_test","SignedShareBase_test","NewViewMsg_test","metric_tests","RawMemoryPool_test","kvbc_dbadapter_test","trc_rpc_use_tests","ccron_table_test","ccron_ticks_generator_test","PartialCommitProofMsg_test","ReplicaStatusMsg_test","sha_hash_tests","secrets_manager_test","TestVectorOfShares","multiplex_comm_test","sparse_merkle_tree_test","bft_client_test","trc_hash_tests","KeyStore_test","SequenceWithActiveWindow_tests","thread_pool_test","order_test","replica_resources_test","sparse_merkle_storage_serialization_unit_test","sparse_merkle_internal_node_test","sliver_test","serializable_test","timers_tests","RollingAvgAndVar_test","hex_tools_test","callback_registry_test","scope_exit_test","lru_cache_test","simple_memory_pool_test","synchronized_value_test","utilization_test","TestMisc","histogram_tests"]
                 }

# Auto Generated targets
TEST_GROUPS[3] += ["${appName}", "${test_name}", "RVT_test"]

#TODO: Why is it commented out?
COMMENTED_OUT = ['persistency_test', "KeyManager_test"]


def remove_set_property_lines(cmakelists_path: Path):
    original_content = cmakelists_path.read_text()
    start_str = 'set_tests_properties('
    statements_to_remove = []
    content = original_content
    while True:
        start_index = content.find(start_str)
        if start_index == -1:
            break
        end_index = content[start_index:].find(')\n') + start_index
        assert end_index
        statements_to_remove.append(content[start_index:end_index])

    content = original_content
    for set_property_statement in statements_to_remove:
        content = content.replace(set_property_statement, '')

    cmakelists_path.write_text(content)


def add_test_label_property(cmakelists_path: Path):
    original_content = cmakelists_path.read_text()
    start_str = 'add_test('
    content = original_content
    while True:
        start_index = content.find(start_str)
        if start_index == -1:
            break
        end_index = content[start_index:].find(')') + start_index
        assert end_index
        add_test_data = content[start_index + len(start_str):end_index]
        test_name = add_test_data.split(" ")[1] if "NAME" in add_test_data else add_test_data.split(" ")[0]
        test_group = None
        for group_id, group_members in TEST_GROUPS.items():
            if test_name in group_members:
                test_group = group_id
                break

        set_properties_line = ""
        if test_name in COMMENTED_OUT:
            test_group = "COMMENTED_OUT"
            set_properties_line += "#"

        assert test_group, f"Test name {test_name} in file {cmakelists_path} has not group"

        data_to_replace = content[start_index: end_index + 1]
        print(f"before:\n{data_to_replace}")
        set_properties_line += f"set_tests_properties({test_name} PROPERTIES LABELS \"GROUP_{test_group}\")"
        new_data = F"{data_to_replace}\n{set_properties_line}\n"

        content = content.replace(data_to_replace, new_data)
        print(f"after:\n{new_data}")
        content = content[start_index + 1:]

    cmakelists_path.write_text(original_content)


def main():
    root_dir = Path(sys.argv[1])
    for root, dirs, files in os.walk(str(root_dir), topdown=False):
        for name in files:
            if name == 'CMakeLists.txt':
                print(os.path.join(root, name))
                add_test_label_property(Path(os.path.join(root, name)))


if __name__ == "__main__":
    main()