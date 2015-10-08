/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Utilities
 * Purpose:  GDAL scattered data gridding (interpolation) tool
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, Even Rouault <even dot rouault at spatialys dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_string.h"
#include "commonutils.h"
#include "gdal_utils_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage(const char* pszErrorMsg = NULL)

{
    printf( 
        "Usage: gdal_grid [--help-general] [--formats]\n"
        "    [-ot {Byte/Int16/UInt16/UInt32/Int32/Float32/Float64/\n"
        "          CInt16/CInt32/CFloat32/CFloat64}]\n"
        "    [-of format] [-co \"NAME=VALUE\"]\n"
        "    [-zfield field_name] [-z_increase increase_value] [-z_multiply multiply_value]\n"
        "    [-a_srs srs_def] [-spat xmin ymin xmax ymax]\n"
        "    [-clipsrc <xmin ymin xmax ymax>|WKT|datasource|spat_extent]\n"
        "    [-clipsrcsql sql_statement] [-clipsrclayer layer]\n"
        "    [-clipsrcwhere expression]\n"
        "    [-l layername]* [-where expression] [-sql select_statement]\n"
        "    [-txe xmin xmax] [-tye ymin ymax] [-outsize xsize ysize]\n"
        "    [-a algorithm[:parameter1=value1]*]"
        "    [-q]\n"
        "    <src_datasource> <dst_filename>\n"
        "\n"
        "Available algorithms and parameters with their's defaults:\n"
        "    Inverse distance to a power (default)\n"
        "        invdist:power=2.0:smoothing=0.0:radius1=0.0:radius2=0.0:angle=0.0:max_points=0:min_points=0:nodata=0.0\n"
        "    Inverse distance to a power with nearest neighbor search\n"
        "        invdistnn:power=2.0:radius=1.0:max_points=12:min_points=0:nodata=0\n"
        "    Moving average\n"
        "        average:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "    Nearest neighbor\n"
        "        nearest:radius1=0.0:radius2=0.0:angle=0.0:nodata=0.0\n"
        "    Various data metrics\n"
        "        <metric name>:radius1=0.0:radius2=0.0:angle=0.0:min_points=0:nodata=0.0\n"
        "        possible metrics are:\n"
        "            minimum\n"
        "            maximum\n"
        "            range\n"
        "            count\n"
        "            average_distance\n"
        "            average_distance_pts\n"
        "    Linear\n"
        "        linear:radius=-1.0:nodata=0.0\n"
        "\n");

    if( pszErrorMsg != NULL )
        fprintf(stderr, "\nFAILURE: %s\n", pszErrorMsg);

    GDALDestroyDriverManager();
    exit( 1 );
}
/************************************************************************/
/*                         GDALGridOptionsForBinaryNew()                */
/************************************************************************/

static GDALGridOptionsForBinary *GDALGridOptionsForBinaryNew(void)
{
    return (GDALGridOptionsForBinary*) CPLCalloc(  1, sizeof(GDALGridOptionsForBinary) );
}

/************************************************************************/
/*                         GDALGridOptionsForBinaryFree()               */
/************************************************************************/

static void GDALGridOptionsForBinaryFree( GDALGridOptionsForBinary* psOptionsForBinary )
{
    if( psOptionsForBinary )
    {
        CPLFree(psOptionsForBinary->pszSource);
        CPLFree(psOptionsForBinary->pszDest);
        CPLFree(psOptionsForBinary->pszFormat);
        CPLFree(psOptionsForBinary);
    }
}
/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main(int argc, char** argv)
{
    /* Check strict compilation and runtime library version as we use C++ API */
    if (! GDAL_CHECK_VERSION(argv[0]))
        exit(1);

    EarlySetConfigOptions(argc, argv);

/* -------------------------------------------------------------------- */
/*      Generic arg processing.                                         */
/* -------------------------------------------------------------------- */
    GDALAllRegister();
    argc = GDALGeneralCmdLineProcessor( argc, &argv, 0 );
    if( argc < 1 )
        exit( -argc );
    
    for( int i = 0; i < argc; i++ )
    {
        if( EQUAL(argv[i], "--utility_version") )
        {
            printf("%s was compiled against GDAL %s and is running against GDAL %s\n",
                   argv[0], GDAL_RELEASE_NAME, GDALVersionInfo("RELEASE_NAME"));
            return 0;
        }
        else if( EQUAL(argv[i],"--help") )
        {
            Usage();
        }
    }

    GDALGridOptionsForBinary* psOptionsForBinary = GDALGridOptionsForBinaryNew();
    GDALGridOptions *psOptions = GDALGridOptionsNew(argv + 1, psOptionsForBinary);
    CSLDestroy( argv );

    if( psOptions == NULL )
    {
        Usage();
    }

    if( !(psOptionsForBinary->bQuiet) )
    {
        GDALGridOptionsSetProgress(psOptions, GDALTermProgress, NULL);
    }

    if( psOptionsForBinary->pszSource == NULL )
        Usage("No input file specified.");
    if( psOptionsForBinary->pszDest== NULL )
        Usage("No output file specified.");

    if( psOptionsForBinary->pszDest == NULL )
        psOptionsForBinary->pszDest = CPLStrdup(psOptionsForBinary->pszSource);
    else if (!psOptionsForBinary->bQuiet && !psOptionsForBinary->bFormatExplicitlySet)
        CheckExtensionConsistency(psOptionsForBinary->pszDest, psOptionsForBinary->pszFormat);

/* -------------------------------------------------------------------- */
/*      Open input file.                                                */
/* -------------------------------------------------------------------- */
    GDALDatasetH hInDS = GDALOpenEx( psOptionsForBinary->pszSource, GDAL_OF_VECTOR | GDAL_OF_VERBOSE_ERROR,
                                     NULL, NULL, NULL );
    if( hInDS == NULL )
        exit( 1 );

    int bUsageError = FALSE;
    GDALDatasetH hOutDS = GDALGrid(psOptionsForBinary->pszDest,
                                   hInDS,
                                   psOptions, &bUsageError);
    if(bUsageError == TRUE)
        Usage();
    int nRetCode = (hOutDS) ? 0 : 1;
    
    GDALClose(hInDS);
    GDALClose(hOutDS);
    GDALGridOptionsFree(psOptions);
    GDALGridOptionsForBinaryFree(psOptionsForBinary);

    OGRCleanupAll();
    GDALDestroyDriverManager();

    return nRetCode;
}