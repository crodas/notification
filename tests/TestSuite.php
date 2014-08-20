<?php

require __DIR__ . "/basicTest.php";

class TestSuite extends PHPUnit_Framework_TestSuite
{
    public static function suite()
    {
        $suite = new TestSuite('Test test suite');
        $suite->addTestSuite('basicTest');

        return $suite;
    }
}

