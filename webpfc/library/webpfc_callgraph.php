<?php 

/**
 * Send an HTTP header with the response. You MUST use this function instead
 * of header() so that we can debug header issues because they're virtually
 * impossible to debug otherwise. If you try to commit header(), SVN will
 * reject your commit.
 *
 * @param string  HTTP header name, like 'Location'
 * @param string  HTTP header value, like 'http://www.example.com/'
 *
 */
function webpfc_http_header($name, $value) {

	if (!$name) {
		error_log('http_header usage');
		return null;
	}

	if (!is_string($value)) {
		error_log('http_header value not a string');
	}

	header($name.': '.$value, true);
}

/**
 * Genearte and send MIME header for the output image to client browser.
 *
 */
function webpfc_generate_mime_header($type, $length) {
	switch ($type) {
		case 'jpg':
			$mime = 'image/jpeg';
			break;
		case 'gif':
			$mime = 'image/gif';
			break;
		case 'png':
			$mime = 'image/png';
			break;
		case 'ps':
			$mime = 'application/postscript';
		default:
			$mime = false;
	}

	if ($mime) {
		webpfc_http_header('Content-type', $mime);
		webpfc_http_header('Content-length', (string)$length);
	}
}


function webpfc_get_possible_metrics() {
	static $possible_metrics =
	array("wt" => array("Wall", "microsecs", "walltime" ),
			"ut" => array("User", "microsecs", "user cpu time" ),
			"st" => array("Sys", "microsecs", "system cpu time"),
			"cpu" => array("Cpu", "microsecs", "cpu time"),
			"mu" => array("MUse", "bytes", "memory usage"),
			"pmu" => array("PMUse", "bytes", "peak memory usage"),
			"samples" => array("Samples", "samples", "cpu time"));
	return $possible_metrics;
}

function webpfc_get_metrics($xhprof_data) {

	// get list of valid metrics
	$possible_metrics = webpfc_get_possible_metrics();

	// return those that are present in the raw data.
	// We'll just look at the root of the subtree for this.
	$metrics = array();
	foreach ($possible_metrics as $metric => $desc) {
		if (isset($xhprof_data["main()"][$metric])) {
			$metrics[] = $metric;
		}
	}

	return $metrics;
}




/**
 * Takes a parent/child function name encoded as
 * "a==>b" and returns array("a", "b").
 *
 */
function webpfc_parse_parent_child($parent_child) {
	$ret = explode("==>", $parent_child);

	// Return if both parent and child are set
	if (isset($ret[1])) {
		return $ret;
	}

	return array(null, $ret[0]);
}

/**
 * Given parent & child function name, composes the key
 * in the format present in the raw data.
 *
 */
function webpfc_build_parent_child_key($parent, $child) {
	if ($parent) {
		return $parent . "==>" . $child;
	} else {
		return $child;
	}
}


/*
 * Get the children list of all nodes.
*/
function webpfc_get_children_table($raw_data) {
	$children_table = array();
	foreach ($raw_data as $parent_child => $info) {
		list($parent, $child) = webpfc_parse_parent_child($parent_child);
		if (!isset($children_table[$parent])) {
			$children_table[$parent] = array($child);
		} else {
			$children_table[$parent][] = $child;
		}
	}
	return $children_table;
}

/**
 * Compute inclusive metrics for function. This code was factored out
 * of webpfc_compute_flat_info().
 *
 *
 */
function webpfc_compute_inclusive_times($raw_data) {
	
	$display_calls = 1;

	$metrics = webpfc_get_metrics($raw_data);

	$symbol_tab = array();

	/*
	 * First compute inclusive time for each function and total
	* call count for each function across all parents the
	* function is called from.
	*/
	foreach ($raw_data as $parent_child => $info) {

		list($parent, $child) = webpfc_parse_parent_child($parent_child);

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
				$symbol_tab[$child] = array("ct" => $info["ct"]);
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



/**
 * Analyze hierarchical raw data, and compute per-function (flat)
 * inclusive and exclusive metrics.
 *
 * Also, store overall totals in the 2nd argument.
 *
 * @param  array $raw_data          XHProf format raw profiler data.
 * @param  array &$overall_totals   OUT argument for returning
 *                                  overall totals for various
 *                                  metrics.
 * @return array Returns a map from function name to its
 *               call count and inclusive & exclusive metrics
 *               (such as wall time, etc.).
 *
 */
function webpfc_compute_flat_info($raw_data, &$overall_totals) {

	$display_calls = 1;

	$metrics = webpfc_get_metrics($raw_data);

	$overall_totals = array( "ct" => 0,
			"wt" => 0,
			"ut" => 0,
			"st" => 0,
			"cpu" => 0,
			"mu" => 0,
			"pmu" => 0,
			"samples" => 0
	);

	// compute inclusive times for each function
	$symbol_tab = webpfc_compute_inclusive_times($raw_data);

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

	/* adjust exclusive times by deducting inclusive time of children */
	foreach ($raw_data as $parent_child => $info) {
		list($parent, $child) = webpfc_parse_parent_child($parent_child);

		if ($parent) {
			foreach ($metrics as $metric) {
				// make sure the parent exists hasn't been pruned.
				if (isset($symbol_tab[$parent])) {
					$symbol_tab[$parent]["excl_" . $metric] -= $info[$metric];
				}
			}
		}
	}

	return $symbol_tab;
}

/**
 * Generate DOT script from the given raw  data.
 *
 * @param raw_data, php rofile data.
 * @param threshold, float, the threshold value [0,1). The functions in the
 *                   raw_data whose exclusive wall times ratio are below the
 *                   threshold will be filtered out and won't apprear in the
 *                   generated image.
 * @param func, string, the focus function.
 * @param critical_path, bool, whether or not to display critical path with
 *                             bold lines.
 * @returns, string, the DOT script to generate image.
 *
 */
function webpfc_generate_dot_script($raw_data, $func, $threshold, $critical_path, $right=null,
		$left=null) {

	$max_width = 5;
	$max_height = 3.5;
	$max_fontsize = 35;
	$max_sizing_ratio = 20;

	$totals;

	if ($left === null) {
		// init_metrics($raw_data, null, null);
	}
	$sym_table = webpfc_compute_flat_info($raw_data, $totals);

	if ($critical_path) {
		$children_table = webpfc_get_children_table($raw_data);
		$node = "main()";
		$path = array();
		$path_edges = array();
		$visited = array();
		while ($node) {
			$visited[$node] = true;
			if (isset($children_table[$node])) {
				$max_child = null;
				foreach ($children_table[$node] as $child) {

					if (isset($visited[$child])) {
						continue;
					}
					if ($max_child === null ||
							abs($raw_data[webpfc_build_parent_child_key($node,
									$child)]["wt"]) >
							abs($raw_data[webpfc_build_parent_child_key($node,
									$max_child)]["wt"])) {
						$max_child = $child;
					}
				}
				if ($max_child !== null) {
					$path[$max_child] = true;
					$path_edges[webpfc_build_parent_child_key($node, $max_child)] = true;
				}
				$node = $max_child;
			} else {
				$node = null;
			}
		}
	}


	// use the function to filter out irrelevant functions.
	if (!empty($func)) {
		$interested_funcs = array();
		foreach ($raw_data as $parent_child => $info) {
			list($parent, $child) = webpfc_parse_parent_child($parent_child);
			if ($parent == $func || $child == $func) {
				$interested_funcs[$parent] = 1;
				$interested_funcs[$child] = 1;
			}
		}
		foreach ($sym_table as $symbol => $info) {
			if (!array_key_exists($symbol, $interested_funcs)) {
				unset($sym_table[$symbol]);
			}
		}
	}

	$result = "digraph call_graph {\n";

	// Filter out functions whose exclusive time ratio is below threshold, and
	// also assign a unique integer id for each function to be generated. In the
	// meantime, find the function with the most exclusive time (potentially the
	// performance bottleneck).
	$cur_id = 0; $max_wt = 0;
	foreach ($sym_table as $symbol => $info) {
		if (empty($func) && abs($info["wt"] / $totals["wt"]) < $threshold) {
			unset($sym_table[$symbol]);
			continue;
		}
		if ($max_wt == 0 || $max_wt < abs($info["excl_wt"])) {
			$max_wt = abs($info["excl_wt"]);
		}
		$sym_table[$symbol]["id"] = $cur_id;
		$cur_id ++;
	}

	// Generate all nodes' information.
	foreach ($sym_table as $symbol => $info) {
		if ($info["excl_wt"] == 0) {
			$sizing_factor = $max_sizing_ratio;
		} else {
			$sizing_factor = $max_wt / abs($info["excl_wt"]) ;
			if ($sizing_factor > $max_sizing_ratio) {
				$sizing_factor = $max_sizing_ratio;
			}
		}
		$fillcolor = (($sizing_factor < 1.5) ?
				", style=filled, fillcolor=red" : "");

		if ($critical_path) {
			// highlight nodes along critical path.
			if (!$fillcolor && array_key_exists($symbol, $path)) {
				$fillcolor = ", style=filled, fillcolor=yellow";
			}
		}

		$fontsize =", fontsize="
		.(int)($max_fontsize / (($sizing_factor - 1) / 10 + 1));

		$width = ", width=".sprintf("%.1f", $max_width / $sizing_factor);
		$height = ", height=".sprintf("%.1f", $max_height / $sizing_factor);

		if ($symbol == "main()") {
			$shape = "octagon";
			$name ="Total: ".($totals["wt"]/1000.0)." ms\\n";
			$name .= addslashes(isset($page) ? $page : $symbol);
		} else {
			$shape = "box";
			$name = addslashes($symbol)."\\nInc: ". sprintf("%.3f",$info["wt"]/1000) .
			" ms (" . sprintf("%.1f%%", 100 * $info["wt"]/$totals["wt"]).")";
		}
		if ($left === null) {
			$label = ", label=\"".$name."\\nExcl: "
			.(sprintf("%.3f",$info["excl_wt"]/1000.0))." ms ("
			.sprintf("%.1f%%", 100 * $info["excl_wt"]/$totals["wt"])
			. ")\\n".$info["ct"]." total calls\"";
		} else {
			if (isset($left[$symbol]) && isset($right[$symbol])) {
				$label = ", label=\"".addslashes($symbol).
				"\\nInc: ".(sprintf("%.3f",$left[$symbol]["wt"]/1000.0))
				." ms - "
				.(sprintf("%.3f",$right[$symbol]["wt"]/1000.0))." ms = "
				.(sprintf("%.3f",$info["wt"]/1000.0))." ms".
				"\\nExcl: "
				.(sprintf("%.3f",$left[$symbol]["excl_wt"]/1000.0))
				." ms - ".(sprintf("%.3f",$right[$symbol]["excl_wt"]/1000.0))
				." ms = ".(sprintf("%.3f",$info["excl_wt"]/1000.0))." ms".
				"\\nCalls: ".(sprintf("%.3f",$left[$symbol]["ct"]))." - "
				.(sprintf("%.3f",$right[$symbol]["ct"]))." = "
				.(sprintf("%.3f",$info["ct"]))."\"";
			} else if (isset($left[$symbol])) {
				$label = ", label=\"".addslashes($symbol).
				"\\nInc: ".(sprintf("%.3f",$left[$symbol]["wt"]/1000.0))
				." ms - 0 ms = ".(sprintf("%.3f",$info["wt"]/1000.0))
				." ms"."\\nExcl: "
				.(sprintf("%.3f",$left[$symbol]["excl_wt"]/1000.0))
				." ms - 0 ms = "
				.(sprintf("%.3f",$info["excl_wt"]/1000.0))." ms".
				"\\nCalls: ".(sprintf("%.3f",$left[$symbol]["ct"]))." - 0 = "
				.(sprintf("%.3f",$info["ct"]))."\"";
			} else {
				$label = ", label=\"".addslashes($symbol).
				"\\nInc: 0 ms - "
				.(sprintf("%.3f",$right[$symbol]["wt"]/1000.0))
				." ms = ".(sprintf("%.3f",$info["wt"]/1000.0))." ms".
				"\\nExcl: 0 ms - "
				.(sprintf("%.3f",$right[$symbol]["excl_wt"]/1000.0))
				." ms = ".(sprintf("%.3f",$info["excl_wt"]/1000.0))." ms".
				"\\nCalls: 0 - ".(sprintf("%.3f",$right[$symbol]["ct"]))
				." = ".(sprintf("%.3f",$info["ct"]))."\"";
			}
		}
		$result .= "N" . $sym_table[$symbol]["id"];
		$result .= "[shape=$shape ".$label.$width
		.$height.$fontsize.$fillcolor."];\n";
	}

	// Generate all the edges' information.
	foreach ($raw_data as $parent_child => $info) {
		list($parent, $child) = webpfc_parse_parent_child($parent_child);

		if (isset($sym_table[$parent]) && isset($sym_table[$child]) &&
				(empty($func) ||
						(!empty($func) && ($parent == $func || $child == $func)) )) {

			$label = $info["ct"] == 1 ? $info["ct"]." call" : $info["ct"]." calls";

			$headlabel = $sym_table[$child]["wt"] > 0 ?
			sprintf("%.1f%%", 100 * $info["wt"]
					/ $sym_table[$child]["wt"])
					: "0.0%";

			$taillabel = ($sym_table[$parent]["wt"] > 0) ?
			sprintf("%.1f%%",
					100 * $info["wt"] /
					($sym_table[$parent]["wt"] - $sym_table["$parent"]["excl_wt"]))
					: "0.0%";

			$linewidth= 1;
			$arrow_size = 1;

			if ($critical_path &&
					isset($path_edges[webpfc_build_parent_child_key($parent, $child)])) {
				$linewidth = 10; $arrow_size=2;
			}

			$result .= "N" . $sym_table[$parent]["id"] . " -> N"
			. $sym_table[$child]["id"];
			$result .= "[arrowsize=$arrow_size, style=\"setlinewidth($linewidth)\","
			." label=\""
			.$label."\", headlabel=\"".$headlabel
			."\", taillabel=\"".$taillabel."\" ]";
			$result .= ";\n";

		}
	}
	$result = $result . "\n}";

	return $result;
}

/**
 * Generate image according to DOT script. This function will spawn a process
 * with "dot" command and pipe the "dot_script" to it and pipe out the
 * generated image content.
 *
 * @param dot_script, string, the script for DOT to generate the image.
 * @param type, one of the supported image types, see image_types.
 * @returns, binary content of the generated image on success. empty string on
 *           failure.
 *
 */
function webpfc_generate_image_by_dot($dot_script, $type) {
	$descriptorspec = array(
			// stdin is a pipe that the child will read from
			0 => array("pipe", "r"),
			// stdout is a pipe that the child will write to
			1 => array("pipe", "w"),
			// stderr is a file to write to
			2 => array("file", "/dev/null", "a")
	);

	$cmd = " dot -T".$type;

	$process = proc_open($cmd, $descriptorspec, $pipes, "/tmp", array());

	if (is_resource($process)) {
		fwrite($pipes[0], $dot_script);
		fclose($pipes[0]);

		$output = stream_get_contents($pipes[1]);
		fclose($pipes[1]);

		proc_close($process);
		return $output;
	}
	print "failed to shell execute cmd=\"$cmd\"\n";
	exit();
}

/**
 * Generate image content from phprof run id.
 *
 */
function webpfc_get_content_by_run($raw_data, $func, $type,$threshold, $critical) {

	if (!$raw_data) {
		error_log("Raw data is empty");
		return "";
	}

	$script = webpfc_generate_dot_script($raw_data, $func, $threshold , $critical);

	$content = webpfc_generate_image_by_dot($script, $type);
	return $content;
}

/**
 * Generate image from phprof run id and send it to client.
 *
 */
function webpfc_render_image($raw_data,$func,$type,$threshold,$critical) {

	$content = webpfc_get_content_by_run($raw_data, $func, $type, $threshold, $critical);
	if (!$content) {
		print "Error: either we can not find profile data for run_id ".$run_id
		." or the threshold ".$threshold." is too small or you do not"
		." have 'dot' image generation utility installed.";
		exit();
	}

	webpfc_generate_mime_header($type, strlen($content));
	echo $content;
}




/**
 * Type definitions for URL params
 */
define('WEBPFC_STRING_PARAM', 1);
define('WEBPFC_UINT_PARAM',   2);
define('WEBPFC_FLOAT_PARAM',  3);
define('WEBPFC_BOOL_PARAM',   4);


// Supported ouput format
$webpfc_legal_image_types = array(
		"jpg" => 1,
		"gif" => 1,
		"png" => 1,
		"ps"  => 1,
);

/**
 * Internal helper function used by various
 * xhprof_get_param* flavors for various
 * types of parameters.
 *
 * @param string   name of the URL query string param
 *
 */
function webpfc_get_param_helper($param) {
	$val = null;
	if (isset($_GET[$param]))
		$val = $_GET[$param];
	else if (isset($_POST[$param])) {
		$val = $_POST[$param];
	}
	return $val;
}

/**
 * Extracts value for string param $param from query
 * string. If param is not specified, return the
 * $default value.
 *
 */
function webpfc_get_string_param($param, $default = '') {
	$val = webpfc_get_param_helper($param);

	if ($val === null)
		return $default;

	return $val;
}

/**
 * Extracts value for unsigned integer param $param from
 * query string. If param is not specified, return the
 * $default value.
 *
 * If value is not a valid unsigned integer, logs error
 * and returns null.
 *
 */
function webpfc_get_uint_param($param, $default = 0) {
	$val = webpfc_get_param_helper($param);

	if ($val === null)
		$val = $default;

	// trim leading/trailing whitespace
	$val = trim($val);

	// if it only contains digits, then ok..
	if (ctype_digit($val)) {
		return $val;
	}

	error_log("$param is $val. It must be an unsigned integer.");
	return null;
}


/**
 * Extracts value for a float param $param from
 * query string. If param is not specified, return
 * the $default value.
 *
 * If value is not a valid unsigned integer, logs error
 * and returns null.
 *
 */
function webpfc_get_float_param($param, $default = 0) {
	$val = webpfc_get_param_helper($param);

	if ($val === null)
		$val = $default;

	// trim leading/trailing whitespace
	$val = trim($val);

	// TBD: confirm the value is indeed a float.
	if (true) // for now..
		return (float)$val;

	error_log("$param is $val. It must be a float.");
	return null;
}

/**
 * Extracts value for a boolean param $param from
 * query string. If param is not specified, return
 * the $default value.
 *
 * If value is not a valid unsigned integer, logs error
 * and returns null.
 *
 */
function webpfc_get_bool_param($param, $default = false) {
	$val = webpfc_get_param_helper($param);

	if ($val === null)
		$val = $default;

	// trim leading/trailing whitespace
	$val = trim($val);

	switch (strtolower($val)) {
		case '0':
		case '1':
			$val = (bool)$val;
			break;
		case 'true':
		case 'on':
		case 'yes':
			$val = true;
			break;
		case 'false':
		case 'off':
		case 'no':
			$val = false;
			break;
		default:
			error_log("$param is $val. It must be a valid boolean string.");
			return null;
	}

	return $val;

}




/**
 * Initialize params from URL query string. The function
 * creates globals variables for each of the params
 * and if the URL query string doesn't specify a particular
 * param initializes them with the corresponding default
 * value specified in the input.
 *
 * @params array $params An array whose keys are the names
 *                       of URL params who value needs to
 *                       be retrieved from the URL query
 *                       string. PHP globals are created
 *                       with these names. The value is
 *                       itself an array with 2-elems (the
 *                       param type, and its default value).
 *                       If a param is not specified in the
 *                       query string the default value is
 *                       used.
 */
function webpfc_param_init($params) {
	/* Create variables specified in $params keys, init defaults */
	foreach ($params as $k => $v) {
		switch ($v[0]) {
			case WEBPFC_STRING_PARAM:
				$p = webpfc_get_string_param($k, $v[1]);
				break;
			case WEBPFC_UINT_PARAM:
				$p = webpfc_get_uint_param($k, $v[1]);
				break;
			case WEBPFC_FLOAT_PARAM:
				$p = webpfc_get_float_param($k, $v[1]);
				break;
			case WEBPFC_BOOL_PARAM:
				$p = webpfc_get_bool_param($k, $v[1]);
				break;
			default:
				error_log("Invalid param type passed to webpfc_param_init: "
				. $v[0]);
				exit();
		}

		// create a global variable using the parameter name.
		$GLOBALS[$k] = $p;
	}
}