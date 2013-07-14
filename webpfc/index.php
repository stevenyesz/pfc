<?php
require './config.php';
require './library/FileHandler.php';
// TODO: Errorhandling:
// 		No files, outputdir not writable

function get($param, $default=false){
    return (isset($_GET[$param])? $_GET[$param] : $default);
}

function costCmp($a, $b){
    $a = $a['summedSelfCost'];
    $b = $b['summedSelfCost'];

    if ($a == $b) {
        return 0;
    }
    return ($a > $b) ? -1 : 1;
}

function sort_cbk($a, $b)
{
  $sort_col = 'summedSelfCost';

    // descending sort for all others
    $left = $a[$sort_col];
    $right = $b[$sort_col];

    if ($left == $right)
      return 0;
    return ($left > $right) ? -1 : 1;
}

function getCacheKeys($fileName) {
  $msg = '';
  $fileName = Pfc_Config::storageDir().'cache/'.$fileName;
  if(!$file = fopen($fileName, 'r'))
  {
    $msg = "can't open cache log file $fileName";
    return $msg;
  } 
  
  $enties = array();
  while(!feof($file))
  {
    $entry = fgets($file,4096);
    $keypos = strpos($entry,'|');
    $enties[] = array('fname'=>substr($entry,0,$keypos),'key'=>substr($entry,$keypos+1));
  }
  fclose($file);
  return $enties;
}

function cachefConvertname($fname) {
  $fname = str_replace('php::','',$fname);
  $fname = str_replace('->',':',$fname);
  $fname = str_replace('::', ':', $fname);
  return $fname;
}

function cachefGetFilename() {
  $fileName = Pfc_Config::storageDir().'/'.ini_get('pfc.config_file');
  return $fileName;
}

function cachefGetFunctions() {
  $fileName = cachefGetFilename();
  $functions = file_get_contents($fileName);  
  return $functions;
}

function cachefDelete($fname) {
  $functions = cachefGetFunctions();
  $functions = str_replace($fname."\n","",$functions);
  $fileName = cachefGetFilename();
  $file = fopen($fileName,'w');
  fwrite($file,$functions);
  fclose($file);
  return true;
}

function cachefInsert($fname) {
  $fileName = cachefGetFilename();
  if(($file = fopen($fileName,'r')) === FALSE){
    fclose($file);
    die('Failed to open file '.$fileName);
  }
  
  while(!feof($file)){
    $line = fgets($file,4096);
    if($fname == trim($line)){
      fclose($file);
      die('function '.$fname.' alread cached!');
    }
  }
  fclose($file);
  
  if(is_writable($fileName) || chmod($fileName,0777)){
    if(!$file = fopen($fileName,'a')){
      echo "cant open file $fileName";
    }
    
    if(fwrite($file,$fname."\n") === FALSE){
      echo "cant to write file $fileName";
    }
    
  }else{echo "File $fileName is't writable";}
  
  return true;
}

// Make sure we have a timezone for date functions.
if (ini_get('date.timezone') == '')
    date_default_timezone_set( Pfc_Config::$defaultTimezone );

try {
    switch(get('op')){
        case 'file_list':
            echo json_encode(Pfc_FileHandler::getInstance()->getTraceList());
            break;
        case 'function_list':
            $dataFile = get('dataFile');
            if($dataFile=='0'){
                $files = Pfc_FileHandler::getInstance()->getTraceList();
                $dataFile = $files[0]['filename'];
            }
            
            $profileInfo = Pfc_FileHandler::getInstance()->getProfileData($dataFile,'ms');
            
            $shownTotal = 0;
            $breakdown = array('internal' => 0, 'procedural' => 0, 'class' => 0, 'include' => 0);
            $functions = $profileInfo['rows'];
            $runs = count($functions);
            foreach ($functions as $i => $functionInfo) {
            	
 
            	if (false !== strpos($functionInfo['functionName'], 'php::')) {
            		$breakdown['internal'] += $functionInfo['summedSelfCost'];
            		$humanKind = 'internal';
            		$kind = 'blue';
            	} elseif (false !== strpos($functionInfo['functionName'], 'require_once::') ||
            			false !== strpos($functionInfo['functionName'], 'require::') ||
            			false !== strpos($functionInfo['functionName'], 'include_once::') ||
            			false !== strpos($functionInfo['functionName'], 'include::') ||
            			false !== strpos($functionInfo['functionName'], 'load::') ||
            			false !== strpos($functionInfo['functionName'], 'run_init::')) {
            		$breakdown['include'] += $functionInfo['summedSelfCost'];
            		$humanKind = 'include';
            		$kind = 'grey';
            	} else {
            		if (false !== strpos($functionInfo['functionName'], '->') || false !== strpos($functionInfo['functionName'], '::')) {
            			$breakdown['class'] += $functionInfo['summedSelfCost'];
            			$humanKind = 'class';
            			$kind = 'green';
            		} else {
            			if(isset($breakdown['user'])) $breakdown['user'] += $functionInfo['summedSelfCost'];
            			$humanKind = 'procedural';
            			$kind = 'orange';
            		}
            	}
            	if (!(int)get('hideInternals', 0) || strpos($functionInfo['functionName'], 'php::') === false) {
            		$shownTotal += $functionInfo['summedSelfCost'];
            		$functions[$i] = $functionInfo;
            		$functions[$i]['nr'] = $i;
            		$functions[$i]['kind'] = $kind;
            		$functions[$i]['humanKind'] = $humanKind;
            	}
            
            }
            
            usort($functions,'sort_cbk');
            $overall_totals = $profileInfo['total'];
            
            $remainingCost = $overall_totals['wt']*get('showFraction');

            $cachedFunctions = cachefGetFunctions();
            $result['functions'] = array();
            foreach($functions as $key=>$function){

                $remainingCost -= $function['summedSelfCost'];
                //$function['file'] = urlencode($function['file']);
                //$function['line'] = 5;
                $function['key'] = $key;
                $function['cacheStatus'] = (strpos($cachedFunctions,cachefConvertname($function['functionName'])."\n") === FALSE)?'no cached':'cached';
                $result['functions'][] = $function;
                if($remainingCost<0)
                    break;
            }
            
            $costFormat = get('costFormat');
  			//microsec to msec
            foreach ($result['functions'] as $key =>$function) {
  				$result['functions'][$key]['summedSelfCost'] = $function['summedSelfCost']/1000;
  				$result['functions'][$key]['avgSelfCost'] = $function['avgSelfCost']/1000;
  				$result['functions'][$key]['summedInclusiveCost'] = $function['summedInclusiveCost']/1000;
  			}
  			
            $result['summedInvocationCount'] = $overall_totals['ct'];
            $result['summedRunTime'] = $overall_totals['wt']/1000;
            $result['dataFile'] = $dataFile;
            $result['invokeUrl'] = "invokeUrl not implemented yet";
            $result['runs'] = $runs;
            $result['breakdown'] = $breakdown;
            $result['mtime'] = date(Pfc_Config::$dateFormat,filemtime(Pfc_Config::profileDir().'/'.$dataFile));

            $result['linkToFunctionLine'] = true;
            
            echo json_encode($result);
        break;
        case 'callinfo_list':
        	$dataFile = get('file');
        	$functionName = urldecode(get('functionName'));
        	if($dataFile=='0'){
        		$files = Pfc_FileHandler::getInstance()->getTraceList();
        		$dataFile = $files[0]['filename'];
        	}
        	
        	$profileInfo = Pfc_FileHandler::getInstance()->getProfileData($dataFile,'ms',$functionName);
        	usort($profileInfo['rows'],'sort_cbk');
        	$result = array('calledFrom'=>array(), 'subCalls'=>array());
        	$result['calledByHost'] = false;
        	if(count($profileInfo['callinfo']['parents'])){
        		foreach ($profileInfo['callinfo']['parents'] as $parent){
        			
        			foreach ($profileInfo['rows'] as $key=>$row){
        				if($parent == $row['functionName']){
        					$result['calledFrom'][] = array('functionNr'=>$key,'line'=>$row['line'],'callCount'=>$row['invocationCount'],'summedCallCost'=>$row['summedInclusiveCost'],'file'=>$row['file'],'callerFunctionName'=>$parent);
        					break;
        				}
        			}
        		}
        	}
        	if(count($profileInfo['callinfo']['children'])){
        		foreach ($profileInfo['callinfo']['children'] as $child) {
        			foreach ($profileInfo['rows'] as $key=>$row){
        				if($child == $row['functionName']){
        					$result['subCalls'][] = array('functionNr'=>$key,'line'=>$row['line'],'callCount'=>$row['invocationCount'],'summedCallCost'=>$row['summedInclusiveCost'],'file'=>$row['file'],'callerFunctionName'=>$child);
        					break;
        				}
        			}
        		}
        	}
        	echo json_encode($result);
        break;
        case 'fileviewer':
            $file = get('file');
            $line = get('line');

            if($file && $file!=''){
                $message = '';
                if(!file_exists($file)){
                    $message = $file.' does not exist.';
                } else if(!is_readable($file)){
                    $message = $file.' is not readable.';
                } else if(is_dir($file)){
                    $message = $file.' is a directory.';
                }
            } else {
                $message = 'No file to view';
            }
            require 'templates/fileviewer.phtml';

        break;
        case 'viewcache':
          $fname = get('fname');
          $file = get('file');
          if(!$file) $file = 'cachegrind.out._EKMWiki_index_php_Main_Page';
          $message = '';
          $entries = getCacheKeys($file);
          require 'templates/cacheviewer.phtml';
        break;
        case 'get_cache':
          $key = get('key');
          $m = new Memcached();
          $m->addServer('localhost', 11211);
          $var = $m->get($key);
          if(!$var || $var == NULL ) $var = "key not found!";
          //$val = 'cache value';
          $result = array('status'=>'ok','cache_value'=>$var);
          echo json_encode($result);
          break;
    	case 'version_info':
    		$response = @file_get_contents('http://jokke.dk/webpfcupdate.json?version='.Pfc_Config::$webpfcVersion);
    		echo $response;
    	break;
    	case 'cachef_insert':
    	    $fname = get('fname');
    	    cachefInsert(cachefConvertname($fname));
    	    $result = array('status'=>'ok');
    	    echo json_encode($result);
    	break;
    	case 'cachef_delete':
    	    $fname = get('fname');
    	    cachefDelete(cachefConvertname($fname));
    	    $result = array('status'=>'ok');
    	    echo json_encode($result);
    	break;
    	default:
            $welcome = '';
            if (!file_exists(Pfc_Config::storageDir()) || !is_writable(Pfc_Config::storageDir())) {
                $welcome .= 'Pfc $storageDir does not exist or is not writeable: <code>'.Pfc_Config::storageDir().'</code><br>';
            }

            if ($welcome == '') {
                $welcome = 'Select a cachegrind file above<br>(looking in <code>'.Pfc_Config::storageDir().'</code> for files matching <code>'.Pfc_Config::profileOutputFormat().'</code>)';
            }
            require 'templates/index.phtml';
    }
} catch (Exception $e) {
    echo json_encode(array('error' => $e->getMessage().'<br>'.$e->getFile().', line '.$e->getLine()));
    return;
}


