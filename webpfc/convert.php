<?
function sort_order($a, $b) {
	$left = $a->packingStart;
	$right = $b->packingStart;
	if ($left == $right)
      return 0;
    return ($left < $right) ? -1 : 1;
}

function overlap($prev,$append) {
	$left = $prev->packingStart + $prev->duration;
	$right = $append->packingStart;
	
	return ($left >= $right) ? true : false;
}

function orderTree($idx){
	global $orders,$orderLen;
	
	$order = $orders[$idx];
	
	$maxSubDepth = 0;
	$subOrders = array();
	$next_idx = $idx + 1;

	while($next_idx < $orderLen){
		if(overlap($orders[$idx], $orders[$next_idx])){
			$subTree = orderTree($next_idx);
			$maxSubDepth = max($maxSubDepth, $subTree->depth);
			$next_idx = $subTree->next_idx;
			$subOrders[] = $subTree;
		}else break;	
	}
	
	$order->next_idx = $next_idx;
	$order->subOrders = $subOrders;
	$order->depth = $maxSubDepth + 1;
	
	return $order;
}

function fillMissing($tree,$left,$wide,$deep) {
	global $originalOrders;
	$width = $wide/$tree->depth;
	$originalOrders[$tree->oid]->width = $width;
	$originalOrders[$tree->oid]->left = $left;
	
	if(count($tree->subOrders) > 0){
		foreach ($tree->subOrders as $subTree){
			fillMissing($subTree, $left+$width, $wide-$width, $deep+1);
		}
	}
}

function getResultData($rawData,$wide) {
global $orders,$orderLen,$originalOrders;
//$orders = json_decode($rawData);
$orders = array();
$lines = explode("\n", $rawData,10000);
foreach ($lines as $line){
	list($orderId, $packingStart, $duration) = explode("\t", $line);
	$order = new stdClass();
	$order->orderId = $orderId;
	$order->packingStart = $packingStart;
	$order->duration = $duration;
	$orders[] = $order;
}

$originalOrders = $orders;
$orderLen = count($orders);

foreach ($orders as $key=>$order){
	$order->oid = $key;
}

usort($orders, 'sort_order');
$start = $orders[0]->packingStart;
$maxwidth = $orders[count($orders)-1]->packingStart - $start;
$widthPer = 5500/$maxwidth;

foreach ($orders as $key=>$order){
	$orders[$key]->packingStart = ($order->packingStart - $start)*$widthPer;
	$orders[$key]->duration = ($order->duration)*$widthPer;	
}

$order_tree = array();
$idx = 0;
while($idx < $orderLen)
{
	$tree = orderTree($idx);
	$order_tree[] = $tree;
	fillMissing($tree, 0, $wide, 0);
	$idx = $tree->next_idx;
}

$resultOrders = array();
foreach ($originalOrders as $order)
{
	$resultOrder = new stdClass();
	$resultOrder->orderId = $order->orderId;
	$resultOrder->packingStart = $order->packingStart;
	$resultOrder->duration = $order->duration;
	$resultOrder->width = $order->width;
	$resultOrder->left = $order->left;
	$resultOrders[] = $resultOrder;
}
return json_encode($resultOrders);
}







