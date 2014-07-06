<?php

/**
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
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
require_once "openmediavault/product.inc";
require_once "openmediavault/rpcservice.inc";
require_once "openmediavault/system.inc";
require_once "openmediavault/util.inc";

class OMVRpcServiceSubsonic extends OMVRpcServiceAbstract
{
    private $subsonicSettingsDir = "/var/subsonic/db/";
    private $subsonicSettingsFile = "/var/subsonic/subsonic.properties";

    private function getPluginName()
    {
        return strtolower($this->getName());
    }

    private function getEventMessagePath()
    {
        return sprintf("org.openmediavault.service.%s", $this->getPluginName());
    }

    private function getXPath()
    {
        return sprintf("//services/%s", $this->getPluginName());
    }

    public function getName()
    {
        return "Subsonic";
    }

    public function initialize()
    {
        $this->registerMethod("getSettings");
        $this->registerMethod("setSettings");
        $this->registerMethod('doUpdateSUB');
        $this->registerMethod('doUpdateSUBB');

        $this->registerMethod('doBackup');
        $this->registerMethod("doRestore");
    }

    public function getSettings($params, $context)
    {
        $cmd = sprintf("omv-mkconf %s getsettings", $this->getPluginName());

        if (0 !== $this->exec($cmd, $output)) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_FAILED,
                $cmd,
                implode("\n", $output)
            );
        }

        global $xmlConfig;

        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $object = $xmlConfig->get($this->getXPath());

        if (is_null($object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        $object["enable"]  = boolval($object["enable"]);
        $object["showtab"] = boolval($object["showtab"]);
        $object["update"]  = boolval($object["update"]);
        $object["bupdate"] = boolval($object["bupdate"]);
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
                    "mntentref" : { ' . $GLOBALS['OMV_JSONSCHEMA_UUID_NONE'] . ' },
                    "msg"       : { "type" : "string", "optional" : true },
                    "path"      : { "type" : "string", "optional" : true }
                }
            }'
        );

        $xPath = sprintf("//system/fstab/mntent[uuid='%s']", $params["mntentref"]);
        $deviceDir = $xmlConfig->get($xPath);
        $deviceDir = $deviceDir["dir"];


        $backupPath = "$deviceDir/SUBbackup";
        $params["path"] = $backupPath;


        // Get existing configuration object
        $oldObject = $xmlConfig->get($this->getXPath());

        if (is_null($oldObject)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getXPath()
            );
        }

        $object = array(
            "enable"    => array_boolval($params, "enable"),
            "showtab"   => array_boolval($params, "showtab"),
            "update"    => array_boolval($params, "update"),
            "bupdate"   => array_boolval($params, "bupdate"),
            "port"      => $params["port"],
            "msg"       => $params["msg"],
            "mntentref" => $params["mntentref"],
            "path"      => $params["path"],
        );

        if (false === $xmlConfig->replace($this->getXPath(), $object)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_SET_OBJECT_FAILED,
                $this->getXPath()
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

    public function doBackup($params, $context)
    {
        global $xmlConfig;

        // Subsonic needs to have run once to generate its
        // settings. Check if this is true
        if (!file_exists($this->subsonicSettingsFile)) {
            throw new OMVException(
                OMVErrorMsg::E_EXEC_MISC,
                "No settings folder found!"
            );
        }

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();

        // Parent process.
        if ($pid > 0) {
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus(
                $bgStatusFilename,
                "outputfilename",
                $bgOutputFilename
            );

            // Get the configuration object.
            $xPath = sprintf("%s/path", $this->getXPath());
            $backupPath = $xmlConfig->get($xPath);

            $cmd = sprintf(
                "tar -zcvPf %s -c %s %s 2>&1",
                $backupPath,
                $this->subsonicSettingsFile,
                $this->subsonicSettingsDir
            );

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            exit(0);
        } catch (Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            exit(1);
        }
    }

    public function doRestore($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get the configuration object.
        $xPath = sprintf("%s/path", $this->getXPath());
        $backupPath = $xmlConfig->get($xPath);

        if (is_null($backupPath)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $xPath
            );
        }

        // Check if backup is present.
        if (file_exists($backupPath)) {
            // Create a background process.
            $bgStatusFilename = $this->createBgProcStatus();
            $pid = $this->fork();

            // Parent process.
            if ($pid > 0) {
                $this->initializeBgProcStatus($bgStatusFilename, $pid);
                return $bgStatusFilename;
            }

            // Delete current settings folder
            rmdir($this->subsonicSettingsDir);

            // Get the Subsonic module
            $moduleMgr = &OMVModuleMgr::getInstance();
            $subsonicModule = $moduleMgr->getModule($this->getName());

            // Child process.


            try {
                $subsonicModule->stopService();
                $bgOutputFilename = $this->createBgProcOutput();
                $this->updateBgProcStatus(
                    $bgStatusFilename,
                    "outputfilename",
                    $bgOutputFilename
                );

                $cmd = sprintf("tar -zxvPf %s -C / 2>&1", $backupPath);

                $this->exec($cmd, $output, $bgOutputFilename);
                $this->finalizeBgProcStatus($bgStatusFilename, $output);
                $subsonicModule->startService();
                exit(0);
            } catch (Exception $e) {
                $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
                $subsonicModule->startService();
                exit(1);
            }
        } else {
            throw new OMVException(OMVErrorMsg::E_EXEC_MISC, "No backup found!");
        }
    }

    public function doUpdateSUB($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get the Subsonic module
        $moduleMgr = &OMVModuleMgr::getInstance();
        $subsonicModule = $moduleMgr->getModule($this->getName());

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();

        // Parent process.
        if ($pid > 0) {
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $subsonicModule->stopService();
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus(
                $bgStatusFilename,
                "outputfilename",
                $bgOutputFilename
            );

            $cmd = sprintf("omv-mkconf %s update 2>&1", $this->getPluginName());

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            $subsonicModule->startService();
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            $subsonicModule->startService();
            exit(1);
        }
    }

    public function doUpdateSUBB($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Get the Subsonic module
        $moduleMgr = &OMVModuleMgr::getInstance();
        $subsonicModule = $moduleMgr->getModule($this->getName());

        // Create a background process.
        $bgStatusFilename = $this->createBgProcStatus();
        $pid = $this->fork();

        // Parent process.
        if ($pid > 0) {
            $this->initializeBgProcStatus($bgStatusFilename, $pid);
            return $bgStatusFilename;
        }

        // Child process.
        try {
            $subsonicModule->stopService();
            $bgOutputFilename = $this->createBgProcOutput();
            $this->updateBgProcStatus(
                $bgStatusFilename,
                "outputfilename",
                $bgOutputFilename
            );

            $cmd = sprintf("omv-mkconf %s bupdate 2>&1", $this->getPluginName());

            $this->exec($cmd, $output, $bgOutputFilename);
            $this->finalizeBgProcStatus($bgStatusFilename, $output);
            $subsonicModule->startService();
            exit(0);
        } catch(Exception $e) {
            $this->finalizeBgProcStatus($bgStatusFilename, "", $e);
            $subsonicModule->startService();
            exit(1);
        }
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceSubsonic());
