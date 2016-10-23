<?php

/**
 * Copyright (C) 2010-2012 Ian Moore <imooreyahoo@gmail.com>
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers.
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
 //file_put_contents('filename.txt', print_r($b, true));

namespace OMV\Engined\Rpc;

use DateTime;
use Exception;
use OMV\Config\Database;
use OMV\Engine\Notify\Dispatcher;
use OMV\Rpc\ServiceAbstract;
use OMV\Rpc\ServiceManager as RpcServiceManager;
use OMV\System\Process;
use SplFileObject;

class Subsonic extends ServiceAbstract
{
    private $database;

    private $dataModelPath = 'conf.service.subsonic';

    private $dispatcher;

    private $eventMessagePath = 'org.openmediavault.conf.services.subsonic';

    private static $commands = array("doUpdateSuB","doUpdateSuBB");
	
    private $subsonicSettingsDir = "/var/subsonic/";
	
    private $subsonicSettingsFile = "/var/subsonic/subsonic.properties";
	
    private $subsonicUser = "subsonic";

    public function __construct(Database $database = null, Dispatcher $dispatcher = null)
    {
        $this->database = $database ?: Database::getInstance();
        $this->dispatcher = $dispatcher ?: Dispatcher::getInstance();
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
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $cmd = new Process(sprintf("omv-mkconf %s getsettings", "subsonic"));
        $cmd->setRedirect2to1();
        $cmd->execute();

        $object = $this->database->get($this->dataModelPath);

        return $object->getAssoc();
    }

    public function setSettings($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.subsonic.setsettings');

        // Get existing configuration object
        $oldObject = $this->database->get($this->dataModelPath);

        // Run checks to verify sellected settings
        if ($params['ssl'] == 1) {
            if ($params['ppass'] == 1) {
                throw new Exception('You can not enable both SSL and Proxy Pass.');
            }
            $db = \OMV\Config\Database::getInstance();
            $getInfo = $db->getAssoc("conf.webadmin", $params['enablessl']);
            $xSsl = $getInfo['enablessl'];
            if ($xSsl == "1") {
                throw new Exception('You have SSL enabled for the main UI, Do NOT enable it here as well.');
            }

            $getInfo = $db->getAssoc('conf.system.certificate.ssl');

            if (! array_key_exists('uuid', $getInfo[0])) {
                throw new Exception('You have not generated any SSL certificates.');
            }
        }

        if ($params['showtab'] == 1 && $params['ppass'] != 1) {
            if (! file_exists('/var/www/openmediavault/js/omv/module/admin/service/media/Media.js')) {
                throw new Exception('Install links and add an entry or enable Proxy Pass to use the web tab.');
            }
        }

        $oldObject->setAssoc($params);

        $this->database->set($oldObject);
        $this->dispatcher->notify(OMV_NOTIFY_MODIFY, $this->eventMessagePath, $oldObject);

        return $oldObject->getAssoc();
    }

    public function downloadBackup($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

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
