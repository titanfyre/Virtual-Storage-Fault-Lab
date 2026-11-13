require 'virtual_storage_fault_lab'
client=VirtualStorageFaultLabSDK::Client.new(library_path:ARGV[0]);begin
 begin;client.call('check');raise 'Invalid request accepted';rescue VirtualStorageFaultLabSDK::NativeError;end
 tests=JSON.parse("[{\"operation\": \"make_policy\", \"arguments\": {\"first_block\": \"2048\", \"block_count\": \"8\", \"every\": 2, \"flags\": 3}, \"expected_key\": \"FirstBlock\", \"expected_value\": \"2048\"}]")
 tests.each do |test|
 result=client.call(test['operation'],test['arguments']);value=result[test['expected_key']];raise 'Contract mismatch' if test.key?('expected_value') && value!=test['expected_value'];raise 'Array missing' if test.key?('minimum_length') && value.size<test['minimum_length'];end if ARGV[0]
ensure;client.close;end
