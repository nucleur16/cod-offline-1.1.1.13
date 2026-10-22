package codo.common
{
   import codo.interfaces.IDataManager;
   import codo.objects.BindCallbackObj;
   import codo.objects.ResultObj;
   import flash.utils.Dictionary;
   
   public class DataManager implements IDataManager
   {
      
      private static var _instance:DataManager;
      
      // Decompilation skipped
      
      private var _database:Dictionary;
      
      private var _serviceCallbacks:Dictionary;
      
      private var _requestCallbacks:Dictionary;
      
      private var _bindCallbacks:Dictionary;
      
      private var _dataProviderCallbacks:Dictionary;
      
      private var _currentDataID:* = null;
      
      public function DataManager()
      {
         // Decompilation skipped
      }
      
      public static function getInstance() : DataManager
      {
         // Decompilation skipped
      }
      
      public function bind(dataID:*, callback:Function, params:* = null, immediate:Boolean = true, fromView:Boolean = false, fromController:Boolean = false) : void
      {
         // Decompilation skipped
      }
      
      public function unbind(dataID:*, callback:Function) : void
      {
         // Decompilation skipped
      }
      
      public function unload() : void
      {
         // Decompilation skipped
      }
      
      public function request(dataID:*, params:* = null, callback:Function = null) : void
      {
         // Decompilation skipped
      }
      
      public function register(dataID:*, callback:Function) : void
      {
         // Decompilation skipped
      }
      
      public function addDP(dataID:*, callback:Function) : void
      {
         // Decompilation skipped
      }
      
      public function removeDP(dataID:*, callback:Function) : void
      {
         // Decompilation skipped
      }
      
      public function getDP(dataID:*, params:Array = null) : *
      {
         // Decompilation skipped
      }
      
      public function setBindData(dataID:*, data:Object) : void
      {
         // Decompilation skipped
      }
      
      public function setData(resObj:ResultObj) : void
      {
         // Decompilation skipped
      }
      
      private function isBound(dataID:*, callback:Function) : Boolean
      {
         // Decompilation skipped
      }
      
      private function isRequested(dataID:*, callback:Function) : Boolean
      {
         // Decompilation skipped
      }
      
      private function getCallbackObj(dataID:*, callback:Function, callbacks:Dictionary) : BindCallbackObj
      {
         // Decompilation skipped
      }
      
      private function dispatch(dataID:*, callbackObj:BindCallbackObj) : void
      {
         // Decompilation skipped
      }
      
      private function dispatchToRequestedCallbacks(dataID:*) : void
      {
         // Decompilation skipped
      }
      
      private function dispatchToBoundCallbacks(dataID:*, toController:Boolean) : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

