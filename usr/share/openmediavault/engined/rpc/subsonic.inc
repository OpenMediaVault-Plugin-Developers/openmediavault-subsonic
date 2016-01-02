<?php

/**
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/config.inc";
require_once "openmediavault/error.inc";
require_once "openmediavault/notify.inc";
require_once "openmediavault/object.inc";
require_once "openmediavault/rpcservice.inc";

class OMVRpcServiceSubsonic extends OMVRpcServiceAbstract
{

    private static $commands = array("doUpdateSuB","doUpdateSuBB");
    private $subsonicSettingsDir = "/var/subsonic/";
    private $subsonicSettingsFile = "/var/subsonic/subsonic.properties";
    private $subsonicUser = "subsonic";

    private function getPluginName()
    {
        return strtolower($this->getName());
    }

    private function getEventMessagePath()
    {
        return sprintf("org.openmediavault.service.%s", $this->getPluginName());
    }

    private function getXpath()
    {
        return sprintf("/config/services/%s", $this->getPluginName());
    }

    public function getName()
    {
        return "Subsonic";
    }

    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod("doCommand");

        $this->registerMethod("downloadBackup");
        $this->registerMethod("uploadBackup");
    }

    public function getSettings($params, $context)
    {
        $cmd = sprintf("omv-mkconf %s getsettings", $this->getPluginName());

        if (0 !== $this->exec($cmd, $output)) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode(PHP_EOL, $output)
            );
        }

        global $xmlConfig;

        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $object = $xmlConfig->get($this->getXpath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXpath()
            );
        }

        $object["enable"]  = boolval($object["enable"]);
        $object["showtab"] = boolval($object["showtab"]);
        $object["update"]  = boolval($object["update"]);
        $object["bupdate"] = boolval($object["bupdate"]);
        $object["ssl"]     = boolval($object["ssl"]);
        $object["ppass"]   = boolval($object["ppass"]);
        $object["port"]    = intval($object["port"]);

        return $object;
    }

    public function setSettings($params, $context)
    {
        global $xmlConfig;

        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $this->validateMethodParams(
            $params,
            '{
                "type"       : "object",
                "properties" : {
                    "enable"    : { "type" : "boolean" },
                    "showtab"   : { "type" : "boolean" },
                    "update"    : { "type" : "boolean" },
                    "bupdate"   : { "type" : "boolean" },
                    "port"      : { "type" : "integer","minimum":1024,"maximum":65535},
                    "ppass"     : { "type" : "boolean" },
                    "ssl"       : { "type" : "boolean" },
                    "msg"       : { "type" : "string", "optional" : true }
                }
            }'
        );

        // Run checks to verify sellected settings
        if ($params["ssl"] == "1") {
            if ($params["ppass"] == "1") {
                throw new OMVException(
                    OMVErrorMsg::E_MISC_FAILURE,
                    "You can not enable both SSL and Proxy Pass."
                );
            }
        }

        $object = array(
            "enable"    => array_boolval($params, "enable"),
            "showtab"   => array_boolval($params, "showtab"),
            "update"    => array_boolval($params, "update"),
            "bupdate"   => array_boolval($params, "bupdate"),
            "port"      => $params["port"],
            "ppass"     => array_boolval($params, "ppass"),
            "ssl"       => array_boolval($params, "ssl"),
            "msg"       => $params["msg"],
        );

        if (false === $xmlConfig->replace($this->getXpath(), $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXpath()
            );
        }

        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            $this->getEventMessagePath(),
            $object
        );

        return $object;
    }

    public function downloadBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_EVERYONE
        ));

        if (!file_exists("$this->subsonicSettingsFile")) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_MISC,
                "Nothing to backup found."
            );
        }

        // Get the Subsonic module
        $moduleMgr = &OMVModuleMgr::getInstance();
        $subsonicModule = $moduleMgr->getModule($this->getName());
        $subsonicModule->stopService();

        $tmpFile = sprintf("%s/%s.tar", sys_get_temp_dir(), uniqid());
        $archive = new PharData($tmpFile);

        $archive->addFile($this->subsonicSettingsFile, "subsonic.properties");

        $directoryIterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator(
                "$this->subsonicSettingsDir/db/",
                FilesystemIterator::SKIP_DOTS
            )
        );

        $archive->buildFromIterator(
            $directoryIterator,
            $this->subsonicSettingsDir
        );

        $archive->compress(Phar::GZ);

        // Remove uncompressed tar and set $tmpFile to the compressed.
        unlink($tmpFile);
        $tmpFile = "$tmpFile.gz";

        $now = new DateTime();
        $time = $now->format(DateTime::ISO8601);

        $subsonicModule->startService();

        return array(
            "contenttype" => "application/x-tar",
            "filename"    => "subsonic-$time.tar.gz",
            "filepath"    => $tmpFile,
        );
    }

    public function uploadBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "filename": { "type": "string" },
                    "filepath": { "type": "string" }
                }
            }'
        );

        // Get the Subsonic module
        $moduleMgr = &OMVModuleMgr::getInstance();
        $subsonicModule = $moduleMgr->getModule($this->getName());
        $subsonicModule->stopService();

        // Rename file so that it's recognized by the PharData class.
        $filepath = sprintf("%s.tar.gz", $params["filepath"]);
        rename($params["filepath"], $filepath);

        $directoryIterator = new RecursiveIteratorIterator(
            new RecursiveDirectoryIterator(
                $this->subsonicSettingsDir,
                FilesystemIterator::SKIP_DOTS
            ),
            RecursiveIteratorIterator::CHILD_FIRST
        );

        foreach ($directoryIterator as $file) {
            if ($file->isFile()) {
                unlink($file->getPathname());
            }

            if ($file->isDir()) {
                rmdir($file->getPathname());
            }
        }

        $archive = new PharData($filepath);

        $archive->extractTo($this->subsonicSettingsDir);

        foreach ($directoryIterator as $file) {
            chown($file->getPathname(), $this->subsonicUser);
            chgrp($file->getPathname(), "users");
        }

        // Rename file back to orignal name so that OMV will remove it.
        rename($filepath, $params["filepath"]);

        // Send a modify signal so that the user can trigger a restart.
        //$dispatcher = &OMVNotifyDispatcher::getInstance();
        //$dispatcher->notify(OMV_NOTIFY_MODIFY, $this->getEventMessagePath());
        $subsonicModule->startService();
    }

    public function doCommand($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext(
            $context,
            array( "role" => OMV_ROLE_ADMINISTRATOR )
        );

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams($params, '{
            "type"       : "object",
            "properties" : {
                "command"  : { "type" : "string" }
            }
        }');

        if (!in_array($params['command'], self::$commands))
        {
            throw new OMVException(
                OMVErrorMsg::E_MISC_OPERATION_DENIED,
                sprinf("Unknown command - %s", $params['command'])
            );
        }

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();
        if ($pid > 0)   // Parent process.
        {
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus($bgStatusFilename, "outputfilename", $bgOutputFilename);

            $cmd = sprintf("export LANG=C; omv-mkconf %s %s 2>&1", $this->getPluginName(), $params['command'] );
            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceSubsonic());


