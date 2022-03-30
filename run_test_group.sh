#!/bin/bash
export suite_id=$1

# The suits are divided into ~30 minutes groups, new suites should be added to this file
if [ $suite_id == "group_1" ];
then
  export SUITE_NAMES="skvbc_reconfiguration"
elif [ $suite_id == "group_2" ];
then
  export SUITE_NAMES="skvbc_dbsnapshot_tests skvbc_view_change_tests skvbc_time_service skvbc_basic_tests skvbc_state_transfer_tests skvbc_commit_path_tests skvbc_persistence_tests skvbc_chaotic_startup"
elif [ $suite_id == "group_3" ];
then
  export SUITE_NAMES="skvbc_preexecution_with_result_auth_tests skvbc_client_transaction_signing skvbc_preexecution_tests skvbc_batch_preexecution_tests skvbc_ro_replica_tests sparse_merkle_storage_db_adapter_property_test TestThresholdBls skvbc_backup_restore skvbc_checkpoints skvbc_linearizability_tests SigManager_test thin_replica_server_test skvbc_pyclient_tests skvbc_multi_sig TestBlsBatchVerifier kv_blockchain_db_editor_test skvbc_block_accumulation_tests sparse_merkle_storage_db_adapter_unit_test skvbc_auto_view_change_tests skvbc_consensus_batching bft_client_api_tests bcstatetransfer_tests poll_based_state_client_test native_rocksdb_client_test skvbc_reply_tests categorized_kv_blockchain_unit_test resource_manager_test skvbc_cron block_merkle_latest_ver_cf_migration_test cmf_cppgen_tests s3_client_test skvbc_publish_clients_keys preprocessor_test TestGroupElementSizes replica_state_sync_test trc_byzantine_tests immutable_kv_category_unit_test versioned_kv_category_unit_test util_mt_tests diagnostics_tests multiIO_test replica_state_snapshot_service_test pruning_test block_merkle_category_unit_test sparse_merkle_internal_node_property_test categorized_blocks_unit_test cmf_pygen_tests replica_stream_snapshot_client_tests categorized_blockchain_unit_test cre_test_api TestLagrange TestRelicSerialization TestRelic cc_basic_update_queue_tests incomingMsgsStorage_test metadataStorage_test ViewChangeMsg_test sparse_merkle_base_types_test pruning_reserved_pages_client_test pyclient_tests ClientsManager_test crypto_utils_test ViewChange_tests kvbc_filter_test test_skvbc_history_tracker skvbc_network_partitioning_tests skvbc_byzantine_primary_preexecution_tests test_serialization openssl_crypto_wrapper_test source_selector_test metric_server_tests blockchain_view_test thin_replica_client_tests views_manager_test PrePrepareMsg_test ClientPreProcessRequestMsg_test PreProcessReplyMsg_test PreProcessResultMsg_test seqNumForClientRequest_test RequestThreadPool_test trs_sub_buffer_test ReplicaAsksToLeaveViewMsg_test ControllerWithSimpleHistory_test ClientRequestMsg_test CheckpointMsg_test ReqMissingDataMsg_test StartSlowCommitMsg_test AskForCheckpointMsg_test FullCommitProofMsg_test ReplicaRestartReadyMsg_test TimeServiceResPageClient_test TimeServiceManager_test clientservice-test-yaml_parsing msgsCertificate_test SignedShareBase_test NewViewMsg_test metric_tests RawMemoryPool_test kvbc_dbadapter_test trc_rpc_use_tests ccron_table_test ccron_ticks_generator_test PartialCommitProofMsg_test ReplicaStatusMsg_test sha_hash_tests secrets_manager_test TestVectorOfShares multiplex_comm_test sparse_merkle_tree_test bft_client_test trc_hash_tests KeyStore_test SequenceWithActiveWindow_tests thread_pool_test order_test replica_resources_test sparse_merkle_storage_serialization_unit_test sparse_merkle_internal_node_test sliver_test serializable_test timers_tests RollingAvgAndVar_test hex_tools_test callback_registry_test scope_exit_test lru_cache_test simple_memory_pool_test synchronized_value_test utilization_test TestMisc histogram_tests"
fi

export SUITE_NAMES="pyclient_tests"

n=0
for suite in $SUITE_NAMES; do
  echo "Running test $n of group $suite_id"
  make test-single-suite TEST_NAME=$suite APOLLO_LOG_STDOUT=1
  n=$((n+1))
done

