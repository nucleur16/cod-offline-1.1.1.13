package codo.services
{
   import codo.common.CFunctionContainer;
   import codo.common.CFunctionContainerCommon;
   import codo.common.ServiceBase;
   import codo.constants.LocalizationSetting;
   import codo.constants.ServiceID;
   import codo.objects.VideoObj;
   import codo.utils.ScaleformUtil;
   import flash.utils.Dictionary;
   
   public class SettingsService extends ServiceBase
   {
      
      private static const MAX_TEX_RESOLUTIONS:int = 3;
      
      private static var _instance:SettingsService;
      
      // Decompilation skipped
      
      private var _displayModes:Array = null;
      
      private var _resolutions:Array = null;
      
      private var _aspectRatios:Array = null;
      
      private var _antiAliasings:Array = null;
      
      private var _displayRefreshes:Array = null;
      
      private var _texQualities:Array = null;
      
      private var _texResolutions:Array = null;
      
      private var _normalMapResolutions:Array = null;
      
      private var _specularMapResolutions:Array = null;
      
      private var _ssaos:Array = null;
      
      private var _dynamicLights:Array = null;
      
      private var _texResolutionsDict:Dictionary;
      
      public function SettingsService()
      {
         // Decompilation skipped
      }
      
      public static function getInstance() : SettingsService
      {
         // Decompilation skipped
      }
      
      override public function init() : void
      {
         // Decompilation skipped
      }
      
      public function retrieveVideoSetting() : Object
      {
         // Decompilation skipped
      }
      
      public function applyVideoSetting(cObj:Object) : void
      {
         // Decompilation skipped
      }
      
      public function applyBrightness(cValue:Number) : void
      {
         // Decompilation skipped
      }
      
      public function applyFOV(cValue:int) : void
      {
         // Decompilation skipped
      }
      
      private function getIndexByValue(dataP:Array, cur:*) : uint
      {
         // Decompilation skipped
      }
      
      private function getEnumValues(enum:String) : Array
      {
         // Decompilation skipped
      }
      
      public function listDisplayMode() : void
      {
         // Decompilation skipped
      }
      
      public function listResolution() : void
      {
         // Decompilation skipped
      }
      
      public function listAspectRatio() : void
      {
         // Decompilation skipped
      }
      
      public function listAntiAliasing() : void
      {
         // Decompilation skipped
      }
      
      public function listScreenRefreshRate() : void
      {
         // Decompilation skipped
      }
      
      public function listTextureQuality() : void
      {
         // Decompilation skipped
      }
      
      private function listTextureResolutions() : void
      {
         // Decompilation skipped
      }
      
      private function listNormalMapResolutions() : void
      {
         // Decompilation skipped
      }
      
      private function listSpecularMapResolutions() : void
      {
         // Decompilation skipped
      }
      
      public function listSSAOs() : void
      {
         // Decompilation skipped
      }
      
      public function listDynamicLights() : void
      {
         // Decompilation skipped
      }
      
      private function get currentDisplayMode() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentResolution() : String
      {
         // Decompilation skipped
      }
      
      private function get currentBrightness() : Number
      {
         // Decompilation skipped
      }
      
      private function get currentFOV() : int
      {
         // Decompilation skipped
      }
      
      public function get gammaRange() : Object
      {
         // Decompilation skipped
      }
      
      public function get FOVRange() : Object
      {
         // Decompilation skipped
      }
      
      public function get sensitivityRange() : Object
      {
         // Decompilation skipped
      }
      
      private function get currentAspectRatio() : String
      {
         // Decompilation skipped
      }
      
      private function get currentAASamples() : int
      {
         // Decompilation skipped
      }
      
      private function get currentDisplayRefresh() : String
      {
         // Decompilation skipped
      }
      
      private function get currentVSync() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentShadow() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentSpecular() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentDOF() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentSoftenSmokeEdges() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentRagdoll() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentBulletImpacts() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentTextureQuality() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentTextureRes() : int
      {
         // Decompilation skipped
      }
      
      private function get currentNormalMapRes() : int
      {
         // Decompilation skipped
      }
      
      private function get currentSpecularMapRes() : int
      {
         // Decompilation skipped
      }
      
      private function get currentSSAO() : int
      {
         // Decompilation skipped
      }
      
      private function get currentDynamicLight() : int
      {
         // Decompilation skipped
      }
      
      private function get currentGlowAllowed() : Boolean
      {
         // Decompilation skipped
      }
      
      private function get currentDistortion() : Boolean
      {
         // Decompilation skipped
      }
      
      public function restartVideo() : void
      {
         // Decompilation skipped
      }
   }
}

// Decompilation skipped

