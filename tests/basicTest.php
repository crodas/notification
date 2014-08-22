<?php

require __DIR__ . "/../notifd.php";

function url($url)
{
    return "http://localhost:8000/$url?timeout=10";
}

function get($url)
{
    $data = file_get_contents(url($url));
    return json_decode($data, true);
}

/**
 *  @runTestsInSeparateProcess
 */
class basicTest extends PHPUnit_Framework_TestCase
{
    public function testInfo()
    {
        $info = get("info");
        $this->assertTrue(is_array($info['info']));
    }

    public function testNoMessage()
    {
        $start = microtime(true);
        $data = get("channel/foo/bar");
        $this->assertTrue(microtime(true) - $start < 15);
        $this->assertEquals($data['timeout'], true);
        $this->assertEquals($data['messages'], []);
        $this->assertEquals($data['n'], 0);
        $this->assertEquals($data['channel'], 'foo/bar');
    }

    public static function getChannels()
    {
        $channels = [];
        for ($i=0; $i < 100; $i++) {
            $x = rand();
            $channels[] = ["foo/$i-$x"];
        }

        return $channels;
    }

    /**
     *  @dataProvider getChannels
     */
    public function testMessage($channel)
    {
        $n = new Notifd();
        $x = rand();
        $n->send($channel, ['something' => $channel, 'x' => $x]);
        usleep(50000);

        $start = microtime(true);
        $data = get("channel/$channel");
        $this->assertNotEquals($data['messages'], []);
        $this->assertEquals($data['messages'][0]['something'], $channel);
        $this->assertEquals($data['messages'][0]['x'], $x);
        $this->assertTrue(microtime(true) - $start < 7);
        $this->assertEquals($data['n'], 1);
        $this->assertTrue($data['db']);
        $this->assertEquals($data['channel'], $channel);
    }

    /**
     *  @dataProvider getChannels
     */
    public function testRealtimeMessageMultiSubs($channel)
    {
        $master = curl_multi_init();
        $channel = "mm-$channel";
        $ch = array();
        $total = 10;
        for ($i=0; $i < $total; $i++) {
            $ch[$i] = curl_init(url("channel/" . $channel));
            curl_setopt($ch[$i], CURLOPT_RETURNTRANSFER, true);
            curl_multi_add_handle($master, $ch[$i]);
        }

        curl_multi_exec($master, $running);
        usleep(150000);

        $x = rand();
        $n = new Notifd();
        $n->send($channel, ['something' => $channel, 'x' => $x]);

        do {
            curl_multi_exec($master,$running);
        } while($running > 0);

        $datas = [];
        foreach ($ch as $c) {
            $datas[] = json_decode(curl_multi_getcontent($c), true);
        }

        $this->assertEquals(count($datas), $total);

        foreach ($datas as $data) {
            if (empty($data['messages'])) {
                var_dump($datas);exit;
            }
            $this->assertNotEquals($data['messages'], []);
            $this->assertEquals($data['messages'][0]['something'], $channel);
            $this->assertEquals($data['messages'][0]['x'], $x);
            $this->assertEquals($data['n'], 1);
            $this->assertEquals($data['channel'], $channel);
        }

    }

    /**
     *  @dataProvider getChannels
     */
    public function testRealtimeMessage($channel)
    {
        $master = curl_multi_init();
        $channel = "single-$channel";
        $ch = curl_init(url("channel/" . $channel));
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_multi_add_handle($master, $ch);

        curl_multi_exec($master,$running);
        usleep(150000);

        $x = rand();
        $n = new Notifd();
        $n->send($channel, ['something' => $channel, 'x' => $x]);

        do {
            curl_multi_exec($master,$running);
        } while($running > 0);

        $data = json_decode(curl_multi_getcontent($ch), true);

        $this->assertNotEquals($data['messages'], []);
        $this->assertEquals($data['messages'][0]['something'], $channel);
        $this->assertEquals($data['messages'][0]['x'], $x);
        $this->assertEquals($data['n'], 1);
        $this->assertFalse($data['db']);
        $this->assertEquals($data['channel'], $channel);
    }
}
