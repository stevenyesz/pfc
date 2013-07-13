<?
require './config.php';
require './library/FileHandler.php';
$filename = $_GET['file'];
if($filename=='0'){
   $files = Pfpp_FileHandler::getInstance()->getTraceList();
   $filename = $files[0]['filename'];
}
$format = Pfpp_Config::profileOutputFormat();
$matchs = array();
preg_match($format, $filename, $matchs);
$urlname =  $matchs[3]; 
$tsvFileName = $urlname.'.tsv';
$dataDir = Pfpp_Config::tsvdir();
$realFile = $dataDir.$tsvFileName;

$data = file_get_contents($realFile);
echo $data;

