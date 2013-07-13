<?php

require './config.php';
require './library/FileHandler.php';
require './library/webpfc_callgraph.php';

//$GLOBALS['XHPROF_LIB_ROOT'] = dirname(__FILE__) . '/xhprof_lib';

//include_once $GLOBALS['XHPROF_LIB_ROOT'].'/display/xhprof.php';

ini_set('max_execution_time', 100);

$params = array(// run id param
                'file' => array(WEBPFC_STRING_PARAM, ''),
                // the focus function, if it is set, only directly
                // parents/children functions of it will be shown.
                'func' => array(WEBPFC_STRING_PARAM, ''),
                // image type, can be 'jpg', 'gif', 'ps', 'png'
                'type' => array(WEBPFC_STRING_PARAM, 'png'),
                // only functions whose exclusive time over the total time
                // is larger than this threshold will be shown.
                // default is 0.01.
                'threshold' => array(WEBPFC_FLOAT_PARAM, 0.01),
                // whether to show critical_path
                'critical' => array(WEBPFC_BOOL_PARAM, true),
                );

// pull values of these params, and create named globals for each param
webpfc_param_init($params);

// if invalid value specified for threshold, then use the default
if ($threshold < 0 || $threshold > 1) {
  $threshold = $params['threshold'][1];
}

// if invalid value specified for type, use the default
if (!array_key_exists($type, $webpfc_legal_image_types)) {
  $type = $params['type'][1]; // default image type.
}

if($file=='0'){
	$files = Pfpp_FileHandler::getInstance()->getTraceList();
	$file = $files[0]['filename'];
}

$raw_data = Pfpp_FileHandler::getInstance()->getProfileDataRaw($file,'ms');

webpfc_render_image($raw_data,$func,$type,$threshold,$critical);
