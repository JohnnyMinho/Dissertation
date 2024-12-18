add_artery_feature(test_bicicleta LocationPingCentral.cc LocationPingReporter.cc LocationPingWarner.cc LocationPingWarnerD2D.cc Emergency_Service_MQTT.cc)
generate_opp_message(BlackIceWarning.msg TARGET test_bicicleta DIRECTORY lte_msgs2)
add_opp_run(test_bicicleta DEPENDENCY artery-lte NED_FOLDERS ${CMAKE_CURRENT_SOURCE_DIR})

#add_opp_test(lte-blackice SUFFIX backend CONFIG BlackIce-fingerprint-backend)
#add_opp_test(lte-blackice SUFFIX peer CONFIG BlackIce-fingerprint-peer)
#RsuPingWarner.cc
