//
//     Copyright (C) Pixar. All rights reserved.
//
//     This license governs use of the accompanying software. If you
//     use the software, you accept this license. If you do not accept
//     the license, do not use the software.
//
//     1. Definitions
//     The terms "reproduce," "reproduction," "derivative works," and
//     "distribution" have the same meaning here as under U.S.
//     copyright law.  A "contribution" is the original software, or
//     any additions or changes to the software.
//     A "contributor" is any person or entity that distributes its
//     contribution under this license.
//     "Licensed patents" are a contributor's patent claims that read
//     directly on its contribution.
//
//     2. Grant of Rights
//     (A) Copyright Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free copyright license to reproduce its contribution,
//     prepare derivative works of its contribution, and distribute
//     its contribution or any derivative works that you create.
//     (B) Patent Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free license under its licensed patents to make, have
//     made, use, sell, offer for sale, import, and/or otherwise
//     dispose of its contribution in the software or derivative works
//     of the contribution in the software.
//
//     3. Conditions and Limitations
//     (A) No Trademark License- This license does not grant you
//     rights to use any contributor's name, logo, or trademarks.
//     (B) If you bring a patent claim against any contributor over
//     patents that you claim are infringed by the software, your
//     patent license from such contributor to the software ends
//     automatically.
//     (C) If you distribute any portion of the software, you must
//     retain all copyright, patent, trademark, and attribution
//     notices that are present in the software.
//     (D) If you distribute any portion of the software in source
//     code form, you may do so only under this license by including a
//     complete copy of this license with your distribution. If you
//     distribute any portion of the software in compiled or object
//     code form, you may only do so under a license that complies
//     with this license.
//     (E) The software is licensed "as-is." You bear the risk of
//     using it. The contributors give no express warranties,

#ifndef OSD_GL_DRAW_REGISTRY_H
#define OSD_GL_DRAW_REGISTRY_H

#if not defined(__APPLE__)
    #if defined(_WIN32)
        #include <windows.h>
    #endif
    #include <GL/gl.h>
#else
    #include <OpenGL/gl3.h>
#endif

#include "../version.h"

#include "../far/mesh.h"
#include "../osd/drawRegistry.h"
#include "../osd/vertex.h"

#include <map>

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

struct OsdGLDrawConfig : public OsdDrawConfig {
    OsdGLDrawConfig() : program(0) {}
    virtual ~OsdGLDrawConfig();

    GLuint program;
};

struct OsdGLDrawSourceConfig : public OsdDrawSourceConfig {
    OsdDrawShaderSource commonShader;
    OsdDrawShaderSource vertexShader;
    OsdDrawShaderSource tessControlShader;
    OsdDrawShaderSource tessEvalShader;
    OsdDrawShaderSource geometryShader;
    OsdDrawShaderSource fragmentShader;
};

////////////////////////////////////////////////////////////

class OsdGLDrawRegistryBase {
public:
    typedef OsdPatchDescriptor DescType;
    typedef OsdGLDrawConfig ConfigType;
    typedef OsdGLDrawSourceConfig SourceConfigType;

    virtual ~OsdGLDrawRegistryBase();

protected:
    virtual ConfigType * _NewDrawConfig() { return new ConfigType(); }
    virtual ConfigType *
    _CreateDrawConfig(DescType const & desc,
                      SourceConfigType const * sconfig);

    virtual SourceConfigType * _NewDrawSourceConfig() { return new SourceConfigType(); }
    virtual SourceConfigType *
    _CreateDrawSourceConfig(DescType const & desc);
};

template <class DESC_TYPE = OsdPatchDescriptor,
          class CONFIG_TYPE = OsdGLDrawConfig,
          class SOURCE_CONFIG_TYPE = OsdGLDrawSourceConfig >
class OsdGLDrawRegistry : public OsdGLDrawRegistryBase {
public:
    typedef OsdGLDrawRegistryBase BaseRegistry;

    typedef DESC_TYPE DescType;
    typedef CONFIG_TYPE ConfigType;
    typedef SOURCE_CONFIG_TYPE SourceConfigType;

    typedef std::map<DescType, ConfigType *> ConfigMap;
    typedef std::map<DescType, SourceConfigType *> SourceConfigMap;

public:
    virtual ~OsdGLDrawRegistry() {
        Reset();
    }

    void Reset() {
        for (typename ConfigMap::iterator
                i = _configMap.begin(); i != _configMap.end(); ++i) {
            delete i->second;
        }
        _configMap.clear();
        for (typename SourceConfigMap::iterator
                i = _sourceConfigMap.begin(); i != _sourceConfigMap.end(); ++i) {
            delete i->second;
        }
        _sourceConfigMap.clear();
    }

    // fetch shader config
    ConfigType *
    GetDrawConfig(DescType const & desc) {
        typename ConfigMap::iterator it = _configMap.find(desc);
        if (it != _configMap.end()) {
            return it->second;
        } else {
            ConfigType * config =
                _CreateDrawConfig(desc, GetDrawSourceConfig(desc));
            _configMap[desc] = config;
            return config;
        }
    }

    // fetch text and related defines for patch descriptor
    SourceConfigType *
    GetDrawSourceConfig(DescType const & desc) {
        typename SourceConfigMap::iterator it = _sourceConfigMap.find(desc);
        if (it != _sourceConfigMap.end()) {
            return it->second;
        } else {
            SourceConfigType * sconfig = _CreateDrawSourceConfig(desc);
            _sourceConfigMap[desc] = sconfig;
            return sconfig;
        }
    }

protected:
    virtual ConfigType * _NewDrawConfig() { return new ConfigType(); }
    virtual ConfigType *
    _CreateDrawConfig(DescType const & desc,
                      SourceConfigType const * sconfig) { return NULL; }

    virtual SourceConfigType * _NewDrawSourceConfig() { return new SourceConfigType(); }
    virtual SourceConfigType *
    _CreateDrawSourceConfig(DescType const & desc) { return NULL; }

private:
    ConfigMap _configMap;
    SourceConfigMap _sourceConfigMap;
};

} // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

} // end namespace OpenSubdiv

#endif /* OSD_GL_DRAW_REGISTRY_H */
