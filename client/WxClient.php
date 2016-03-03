<?php


class WxClient{

	protected $fp;
	protected $errno;
	protected $errstr;
	protected $timeoutsec = 2;

	public function connect($host = '127.0.0.1', $port=9529, $timeoutsec=2) {
		$this->timeoutsec = $timeoutsec;
		try{
			$this->fp = fsockopen($host, $port, $this->errno, $this->errstr, $this->timeoutsec);
		}catch(Exception $e) {
			throw new Exception($e->getMessage(), 1);
		}
	}

	public function send($message) {
		fwrite($this->fp, $message);
	}

	public function recv() {
		$s = fread($this->fp, 4096);
		return $s;
	}

	public function request($message){
		$this->send($message);
		$s = '';
		while (false===strpos($s, "\n")) {
			$s .= $this->recv();
		}
		return $s;
	}

	public function close() {
		if (is_resource($this->fp)) {
			fclose($this->fp);
			$this->fp = null;
		}
	}

	public function __destruct() {
		$this->close();
	}
}


/////////////////////////////////////////////////////////usage:

$c = new WxClient;

$c->connect('127.0.0.1', isset($argv[1])?$argv[1]:9527);


$c->send("keep-alive:2000\r\n\r\n");
echo $c->recv();
$c->send("keep-alive:2000\r\n\r\n");
echo $c->recv();

echo $c->request("keep-alive:2000\r\n\r\n");
$c->close();

