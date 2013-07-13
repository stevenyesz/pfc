<?php
/**
 * Class handling access to data-files(original and preprocessed) for webpfc.
 * @author Jacob Oettinger
 * @author Joakim Nygård
 */
class Pfc_FileHandler{
	
	private static $singleton = null;
	/**
	 * @return Singleton instance of the filehandler
	 */
	public static function getInstance(){
		if(self::$singleton==null)
			self::$singleton = new self();
		return self::$singleton;
	}
		
	private function __construct(){
		// Get list of files matching the defined format
		$files = $this->getFiles(Pfc_Config::profileOutputFormat(), Pfc_Config::xdebugOutputDir());
		
		// Get list of preprocessed files
        $prepFiles = $this->getPrepFiles('/\\'.Pfc_Config::$preprocessedSuffix.'$/', Pfc_Config::storageDir());
		// Loop over the preprocessed files.		
		foreach($prepFiles as $fileName=>$prepFile){
			$fileName = str_replace(Pfc_Config::$preprocessedSuffix,'',$fileName);
			
			// If it is older than its corrosponding original: delete it.
			// If it's original does not exist: delete it
			if(!isset($files[$fileName]) || $files[$fileName]['mtime']>$prepFile['mtime'] )
				unlink($prepFile['absoluteFilename']);
			else
				$files[$fileName]['preprocessed'] = true;
		}
		// Sort by mtime
		uasort($files,array($this,'mtimeCmp'));
		
		$this->files = $files;
	}
	
	
	/**
	 * Get the value of the cmd header in $file
	 *
	 * @return void string
	 */	
	private function getInvokeUrl($file){
	    if (preg_match('/.webpfc$/', $file)) 
	        return 'Pfc internal';

		// Grab name of invoked file. 
	    $fp = fopen($file, 'r');
        $invokeUrl = '';
        while ((($line = fgets($fp)) !== FALSE) && !strlen($invokeUrl)){
            if (preg_match('/^cmd: (.*)$/', $line, $parts)){
                $invokeUrl = isset($parts[1]) ? $parts[1] : '';
            }
        }
        fclose($fp);
        if (!strlen($invokeUrl)) 
            $invokeUrl = 'Unknown!';

		return $invokeUrl;
	}
	
	/**
	 * List of files in $dir whose filename has the format $format
	 *
	 * @return array Files
	 */
	private function getFiles($format, $dir){
		$list = preg_grep($format,scandir($dir));
		$files = array();
		
		$scriptFilename = $_SERVER['SCRIPT_FILENAME'];
		
		if (function_exists('xdebug_get_profiler_filename'))
		    $selfFile = realpath(xdebug_get_profiler_filename());
		else 
		    $selfFile = '';
		    
		$this->createTsvDir($dir);
		
		$matchs = array();  
		foreach($list as $file){
			$absoluteFilename = $dir.$file;
			
			preg_match($format, $file, $matchs);
			// Exclude webpfc preprocessed files
			if (false !== strstr($absoluteFilename, Pfc_Config::$preprocessedSuffix))
			    continue;
			
			// Make sure that script never parses the profile currently being generated. (infinite loop)
			if ($selfFile == realpath($absoluteFilename))
				continue;
				
			$invokeUrl = rtrim($this->getInvokeUrl($absoluteFilename));
			if (Pfc_Config::$hidePfcProfiles && $invokeUrl == dirname(dirname(__FILE__)).DIRECTORY_SEPARATOR.'index.php')
			    continue;
			
			$urlname =  $matchs[3];  
			$timestamp = $matchs[2]; 
			
			
			
			if(!isset($files[$urlname])){//first occurence, the lastes one
				$files[$urlname] = array('absoluteFilename' => $absoluteFilename, 
								  'filename' => $file,	
			                      'mtime' => filemtime($absoluteFilename), 
								  'time'  => $matchs[2], 	
			                      'preprocessed' => false, 
			                      'invokeUrl' => $invokeUrl,
			                      'filesize' => $this->bytestostring(filesize($absoluteFilename))
			                );
			}else {//more than one same url files found
				
				$tsvFileName = $urlname.'.tsv';
				$this->createTsvFile($dir, $tsvFileName);
				$totalWt = $this->getTotalWt($absoluteFilename);
				$totalRow = date('Y-m-d H:i:s',$timestamp)."\t".$totalWt;
				$this->apptendTsvData($dir, $tsvFileName, $totalRow);
			
				//move old files to history folder
				if(!file_exists($dir.$urlname)){
					mkdir($dir.$urlname);
				}
				
				if($files[$urlname]['time'] < $timestamp)
					$olderfile = $files[$urlname]['filename'];
				else 
					$olderfile = $file;
					
				if(!file_exists($dir.$urlname.'/'.$olderfile)){
					copy($dir.$olderfile, $dir.$urlname.'/'.$olderfile);
				}
				unlink($dir.$olderfile);
				
				//log total request time into a tsv format file, this is used for d3 area chart
				if($files[$urlname]['time'] < $timestamp) {//update the files entry if this is a new request
					$files[$urlname] = array('absoluteFilename' => $absoluteFilename, 
								  'filename' => $file,	
			                      'mtime' => filemtime($absoluteFilename), 
								  'time'  => $matchs[2], 	
			                      'preprocessed' => false, 
			                      'invokeUrl' => $invokeUrl,
			                      'filesize' => $this->bytestostring(filesize($absoluteFilename))
			                );
				}
				
				
				
				
			}
					
		}		

		return $files;
	}
	
	private function apptendTsvData($baseDir,$tsvFile,$row) {
		$tsvDir = $baseDir.'datatsv';
		$realPath = $tsvDir.'/'.$tsvFile;
		$handle = fopen($realPath, 'a');
		fwrite($handle, $row."\n");
		fclose($handle);
		return true;
	}
	
	private function createTsvFile($baseDir,$fileName) {
		$tsvDir = $baseDir.'datatsv';
		$realPath = $tsvDir.'/'.$fileName;
		if(!file_exists($realPath)){
			$handle = fopen($realPath, 'a');
			fwrite($handle, "date\tTotaltime\n");
			fclose($handle);
		}
		return true;
	}
	
	private function createTsvDir($baseDir){
		$tsvDir = $baseDir.'datatsv';
		if(!file_exists($tsvDir)){
			mkdir($tsvDir);
		}
		
		return true;
	}
	
	/**
	 * List of files in $dir whose filename has the format $format
	 *
	 * @return array Files
	 */
	private function getPrepFiles($format, $dir){
		$list = preg_grep($format,scandir($dir));
		$files = array();
		
		$scriptFilename = $_SERVER['SCRIPT_FILENAME'];
		
		foreach($list as $file){
			$absoluteFilename = $dir.$file;
			
			// Make sure that script does not include the profile currently being generated. (infinite loop)
			if (function_exists('xdebug_get_profiler_filename') && realpath(xdebug_get_profiler_filename())==realpath($absoluteFilename))
				continue;
				
			$files[$file] = array('absoluteFilename' => $absoluteFilename, 
			                      'mtime' => filemtime($absoluteFilename), 
			                      'preprocessed' => true, 
			                      'filesize' => $this->bytestostring(filesize($absoluteFilename))
			                );
		}	
	
		return $files;
	}
	/**
	 * Get list of available trace files. Optionally including traces of the webpfc script it self
	 *
	 * @return array Files
	 */
	public function getTraceList(){
		$result = array();
		foreach($this->files as $fileName=>$file){
			$result[] = array('filename'  => $file['filename'], 
			                  'invokeUrl' => str_replace($_SERVER['DOCUMENT_ROOT'].'/', '', $file['invokeUrl']),
			                  'filesize'  => $file['filesize'],
			                  'mtime'     => date(Pfc_Config::$dateFormat, $file['mtime'])
			            );
		}
		return $result;
	}
	
	/**
	 * Get a trace reader for the specific file.
	 * 
	 * If the file has not been preprocessed yet this will be done first.
	 *
	 * @param string File to read
	 * @param Cost format for the reader
	 * @return Pfc_Reader Reader for $file
	 */
	public function getTraceReader($file, $costFormat){
		$prepFile = Pfc_Config::storageDir().$file.Pfc_Config::$preprocessedSuffix;
		try{
			$r = new Pfc_Reader($prepFile, $costFormat);
		} catch (Exception $e){
			// Preprocessed file does not exist or other error
			Pfc_Preprocessor::parse(Pfc_Config::xdebugOutputDir().$file, $prepFile);
			$r = new Pfc_Reader($prepFile, $costFormat);
		}
		return $r;
	}
	
	public function getProfileDataRaw($file) {
		$realFile = Pfc_Config::xdebugOutputDir().$file;
		if (!file_exists($realFile)) {
     		error_log("Could not find file $realFile");
      		return null;
    	}
		$contents = file_get_contents($realFile);
		return unserialize($contents);
	}

/**
 * Takes a parent/child function name encoded as
 * "a==>b" and returns array("a", "b").
 *
 * @author Kannan
 */
function parse_parent_child($parent_child) {
  $ret = explode("==>", $parent_child);

  // Return if both parent and child are set
  if (isset($ret[1])) {
    return $ret;
  }

  return array(null, $ret[0]);
}
/**
 * Compute inclusive metrics for function. This code was factored out
 * of xhprof_compute_flat_info().
 *
 * The raw data contains inclusive metrics of a function for each
 * unique parent function it is called from. The total inclusive metrics
 * for a function is therefore the sum of inclusive metrics for the
 * function across all parents.
 *
 * @return array  Returns a map of function name to total (across all parents)
 *                inclusive metrics for the function.
 *
 * @author Kannan
 */
function compute_inclusive_times($raw_data) {
  $display_calls = 1;
  $metrics[] = "wt";
  $symbol_tab = array();
  $costFormat = get('costFormat','msec');
  
  /*
   * First compute inclusive time for each function and total
   * call count for each function across all parents the
   * function is called from.
   */
  foreach ($raw_data as $parent_child => $info) {

    list($parent, $child) = $this->parse_parent_child($parent_child);

   
    if ($parent == $child) {
      /*
       * XHProf PHP extension should never trigger this situation any more.
       * Recursion is handled in the XHProf PHP extension by giving nested
       * calls a unique recursion-depth appended name (for example, foo@1).
       */
      error_log("Error in Raw Data: parent & child are both: $parent");
      return;
    }

    if (!isset($symbol_tab[$child])) {

      if ($display_calls) {
        $symbol_tab[$child] = array("ct" => $info["ct"],"file" => $info["file"],"line" => $info["line"]);
      } else {
        $symbol_tab[$child] = array();
      }
      foreach ($metrics as $metric) {
        $symbol_tab[$child][$metric] = $info[$metric];
      }
    } else {
      if ($display_calls) {
        /* increment call count for this child */
        $symbol_tab[$child]["ct"] += $info["ct"];
      }

      /* update inclusive times/metric for this child  */
      foreach ($metrics as $metric) {
        $symbol_tab[$child][$metric] += $info[$metric];
      }
    }
  }

  return $symbol_tab;
}

	
	private function getTotalWt($realFile) {
		$contents = file_get_contents($realFile);
		$rawData = unserialize($contents);
		$totalWt = $rawData['main()']['wt'];
		return $totalWt;
	}
	
	public function getProfileData($file, $costFormat, $functionName="") {
		
		$display_calls = 1;
		
		$raw_data = $this->getProfileDataRaw($file);
		$overall_totals = array( "ct" => 0,
                           "wt" => 0,
                           "ut" => 0,
                           "st" => 0,
                           "cpu" => 0,
                           "mu" => 0,
                           "pmu" => 0,
                           "samples" => 0
                           );
        $metrics[] = "wt";
        $symbol_tab = $this->compute_inclusive_times($raw_data);
        
		/* total metric value is the metric value for "main()" */
  		foreach ($metrics as $metric) {
    		$overall_totals[$metric] = $symbol_tab["main()"][$metric];
  		}

  		/*
   		 * initialize exclusive (self) metric value to inclusive metric value
   		 * to start with.
   		 * In the same pass, also add up the total number of function calls.
   		 */
  		foreach ($symbol_tab as $symbol => $info) {
    		foreach ($metrics as $metric) {
      			$symbol_tab[$symbol]["excl_" . $metric] = $symbol_tab[$symbol][$metric];
    		}
    		if ($display_calls) {
      		/* keep track of total number of calls */
      		$overall_totals["ct"] += $info["ct"];
    		}
  		}

  		$callinfo = array('parents'=>array(),'children'=>array());
  		/* adjust exclusive times by deducting inclusive time of children */
  		foreach ($raw_data as $parent_child => $info) {
    		list($parent, $child) = $this->parse_parent_child($parent_child);
			
    		if(!empty($functionName)){
    			if($parent == $functionName)	$callinfo['children'][] = $child;
    			if($child == $functionName) $callinfo['parents'][] =$parent;
    		}
    		
    		if ($parent) {
      			foreach ($metrics as $metric) {
        		// make sure the parent exists hasn't been pruned.
        		if (isset($symbol_tab[$parent])) {
         			 $symbol_tab[$parent]["excl_" . $metric] -= $info[$metric];
        		}
      			}
    		}
  		}
  
  		$fun_rows = array();
		$i=1;
		$limit=5000;
  		foreach ($symbol_tab as $fname=>$row) {
        	if($i++ > $limit) break;  
			$fun_rows[] = array("functionName"=>$fname,
			"file" =>$row["file"],
			"line" =>$row["line"],
			"invocationCount"=>$row["ct"],
  			"summedSelfCost"=>$row["excl_wt"],
  			"avgSelfCost"=>round($row["excl_wt"]/$row["ct"],2),
  			"summedSelfPercent"=>round(100*($row["excl_wt"]/$overall_totals["wt"]),2),
  			"summedInclusiveCost"=>$row["wt"],
			"summedInclusivePercent"=>round(100*($row["wt"]/$overall_totals["wt"])),2);
  		}
        return array('rows'=>$fun_rows,'total'=>$overall_totals, 'callinfo'=>$callinfo);
	}
	/**
	 * Comparison function for sorting
	 *
	 * @return boolean
	 */
	private function mtimeCmp($a, $b){
		if ($a['mtime'] == $b['mtime'])
		    return 0;

		return ($a['mtime'] > $b['mtime']) ? -1 : 1;
	}
	
	/**
	 * Present a size (in bytes) as a human-readable value
	 *
	 * @param int    $size        size (in bytes)
	 * @param int    $precision    number of digits after the decimal point
	 * @return string
	 */
	private function bytestostring($size, $precision = 0) {
   		$sizes = array('YB', 'ZB', 'EB', 'PB', 'TB', 'GB', 'MB', 'KB', 'B');
		$total = count($sizes);

		while($total-- && $size > 1024) {
		    $size /= 1024;
		}
		return round($size, $precision).$sizes[$total];
    }
}
