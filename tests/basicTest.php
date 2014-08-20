<?php

require __DIR__ . "/../notifd.php";

function get($url)
{
    $data = file_get_contents("http://localhost:8000/$url");
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
        $data = get("channel/foo/bar?timeout=5");
        $this->assertTrue(microtime(true) - $start < 7);
        $this->assertEquals($data['timeout'], true);
        $this->assertEquals($data['messages'], []);
        $this->assertEquals($data['n'], 0);
        $this->assertEquals($data['channel'], 'foo/bar');
    }

    public static function getChannels()
    {
        $channels = [];
        for ($i=0; $i < 50; $i++) {
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
        $n->send($channel, ['something' => $channel]);
        //sleep(1);

        $start = microtime(true);
        $data = get("channel/$channel?timeout=10");
        //var_dump($data);exit;

        $this->assertNotEquals($data['messages'], []);
        $this->assertTrue(microtime(true) - $start < 7);
        $this->assertTrue($data['db']);
        $this->assertEquals($data['n'], 1);
        $this->assertEquals($data['channel'], $channel);
    }
}
