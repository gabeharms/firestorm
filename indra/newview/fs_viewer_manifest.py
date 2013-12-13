import os
import subprocess

class FSViewerManifest:
    def fs_is_64bit_build( self ):
        return self.args.has_key( 'm64' )

    def fs_flavor( self ):
        return self.args['viewer_flavor']  # [oss or hvk]
    
    def fs_splice_grid_substitution_strings( self, subst_strings ):
        ret = subst_strings

        if self.args.has_key( 'grid' ) and self.args['grid'] != None:
          ret[ 'grid' ] = self.args['grid']
          ret[ 'grid_caps' ] = self.args['grid'].upper()
        else:
          ret[ 'grid' ] = ""
          ret[ 'grid_caps' ] = ""

        return ret

    def fs_get_substitution_strings( self ):
        substitution_strings = {
            'version' : '.'.join(self.args['version']),
            'version_short' : '.'.join(self.args['version'][:-1]),
            'version_dashes' : '-'.join(self.args['version']),
            'channel':self.channel(),
            'channel_oneword':self.channel_oneword(),
            'channel_unique':self.channel_unique(),
            'subchannel_underscores':'_'.join(self.channel_unique().split()),
            'app_name' : self.app_name()
        }

        return fs_splice_grid_substitution_strings( substitution_strings )

    def fs_channel_legacy_oneword(self):
        return "".join(self.channel().split())
    
    def fs_sign_win_binaries( self ):
        try:
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\firestorm-bin.exe"],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\slplugin.exe"],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\SLVoice.exe"],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\"+self.final_exe()],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
        except Exception, e:
            print "Couldn't sign final binary. Tried to sign %s" % self.args['configuration']+"\\"+self.final_exe()

    def fs_sign_win_installer( self, substitution_strings ):
        try:
            subprocess.check_call(["signtool.exe","sign","/n","Phoenix","/d","Firestorm","/du","http://www.phoenixviewer.com",self.args['configuration']+"\\"+substitution_strings['installer_file']],stderr=subprocess.PIPE,stdout=subprocess.PIPE)
        except Exception, e:
            print "Working directory: %s" % os.getcwd()
            print "Couldn't sign windows installer. Tried to sign %s" % self.args['configuration']+"\\"+substitution_strings['installer_file']

