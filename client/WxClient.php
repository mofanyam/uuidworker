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

	protected function send($message , $keepalive) {
		fwrite($this->fp, $message);
	}

	protected function recv() {
		$s = fread($this->fp, 4096);
		return $s;
	}

	public function request($message, $keepalive = 2000){
		$this->send($message, $keepalive);
		return $this->recv();
	}

	public function close() {
		if (is_resource($this->fp)) {
			//$this->send('', 0);//server will close the connection Immediately
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

$c->connect('127.0.0.1', 9527);

echo $c->request("abcd\n");

$c->close();