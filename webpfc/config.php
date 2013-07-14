<?php
class Pfc_MasterConfig
{
    static $webpfcVersion = '1.1';
}

/**
 * Configuration for webpfc
 * @author Jacob Oettinger
 * @author Joakim Nygård
 */
class Pfc_Config extends Pfc_MasterConfig {
	/**
	* Automatically check if a newer version of webpfc is available for download
	*/
	static $checkVersion = true;
	static $hidePfcProfiles = true;
		
	/**
	* Suffix for preprocessed files
	*/
	static $preprocessedSuffix = '.webpfc';
	
	static $defaultTimezone = 'Europe/Copenhagen';
	static $dateFormat = 'Y-m-d H:i:s';
	static $defaultCostformat = 'percent'; // 'percent', 'usec' or 'msec'
	static $defaultFunctionPercentage = 90;
	static $defaultHideInternalFunctions = false;

	/**
	* Path to python executable
	*/ 
	static $pythonExecutable = '/usr/bin/python';
	
	/**
	* Path to graphviz dot executable
	*/	
	static $dotExecutable = '/usr/bin/dot';
		
	/**
	* sprintf compatible format for generating links to source files. 
	* %1$s will be replaced by the full path name of the file
	* %2$d will be replaced by the linenumber
	*/
	static $fileUrlFormat = 'index.php?op=fileviewer&file=%1$s#line%2$d'; // Built in fileviewer
	//static $fileUrlFormat = 'txmt://open/?url=file://%1$s&line=%2$d'; // Textmate
	//static $fileUrlFormat = 'file://%1$s'; // ?

    /**
    * format of the trace drop down list                                                                                                                                                      
    * default is: invokeurl (tracefile_name) [tracefile_size]
    * the following options will be replaced:
    *   %i - invoked url
    *   %f - trace file name
    *   %s - size of trace file
    *   %m - modified time of file name (in dateFormat specified above)
    */
    static $traceFileListFormat = '%i (%f) [%s]';


    #########################
    # BELOW NOT FOR EDITING #
    #########################

    static function profileOutputFormat() {
       
    		
	    $outputName = '/^(cachegrind|memoize|pfc)\.out\.(\d*)\.(.+)$/';
	    
    	
	    return $outputName;
    }
	
    static function tsvdir() {
    	return Pfc_Config::profileDir().'/'.'datatsv/';
    }
	
    static function profileDir() {
    	return Pfc_Config::storageDir().'/profile';
    }
	/**
	* Writable dir for information storage
	*/
	static function storageDir() {
	    $dir = ini_get('pfc.data_dir');
	    return realpath($dir);
	}
}
