/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%                              JJJ  PPPP    222                               %
%                               J   P   P  2   2                              %
%                               J   PPPP     22                               %
%                            J  J   P       2                                 %
%                             JJ    P      22222                              %
%                                                                             %
%                                                                             %
%                     Read/Write JPEG-2000 Image Format                       %
%                                                                             %
%                                   Cristy                                    %
%                                Nathan Brown                                 %
%                                 June 2001                                   %
%                                                                             %
%                                                                             %
%  Copyright @ 1999 ImageMagick Studio LLC, a non-profit organization         %
%  dedicated to making software imaging solutions freely available.           %
%                                                                             %
%  You may not use this file except in compliance with the License.  You may  %
%  obtain a copy of the License at                                            %
%                                                                             %
%    https://imagemagick.org/script/license.php                               %
%                                                                             %
%  Unless required by applicable law or agreed to in writing, software        %
%  distributed under the License is distributed on an "AS IS" BASIS,          %
%  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   %
%  See the License for the specific language governing permissions and        %
%  limitations under the License.                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%
*/

/*
  Include declarations.
*/
#include "MagickCore/studio.h"
#include "MagickCore/artifact.h"
#include "MagickCore/attribute.h"
#include "MagickCore/blob.h"
#include "MagickCore/blob-private.h"
#include "MagickCore/cache.h"
#include "MagickCore/colorspace.h"
#include "MagickCore/colorspace-private.h"
#include "MagickCore/color.h"
#include "MagickCore/color-private.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/image.h"
#include "MagickCore/image-private.h"
#include "MagickCore/list.h"
#include "MagickCore/magick.h"
#include "MagickCore/memory_.h"
#include "MagickCore/monitor.h"
#include "MagickCore/monitor-private.h"
#include "MagickCore/option.h"
#include "MagickCore/pixel-accessor.h"
#include "MagickCore/profile-private.h"
#include "MagickCore/property.h"
#include "MagickCore/quantum-private.h"
#include "MagickCore/resource_.h"
#include "MagickCore/semaphore.h"
#include "MagickCore/static.h"
#include "MagickCore/statistic.h"
#include "MagickCore/string_.h"
#include "MagickCore/string-private.h"
#include "MagickCore/module.h"
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
#include <openjpeg.h>

#define OPJ_COMPUTE_NUMERIC_VERSION(major,minor,patch) ((major<<24) | (minor<<16) | (patch<<8) | 0)
#define OPJ_NUMERIC_VERSION OPJ_COMPUTE_NUMERIC_VERSION(OPJ_VERSION_MAJOR,OPJ_VERSION_MINOR,OPJ_VERSION_BUILD)
#endif

/*
  Typedef declarations.
*/
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
typedef struct _JP2CompsInfo
{
  double
    scale;

  ssize_t
    addition,
    pad,
    y_index;
} JP2CompsInfo;
#endif

/*
  Forward declarations.
*/
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
static MagickBooleanType
  WriteJP2Image(const ImageInfo *,Image *,ExceptionInfo *);
#endif

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I s J 2 K                                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsJ2K() returns MagickTrue if the image format type, identified by the
%  magick string, is J2K.
%
%  The format of the IsJ2K method is:
%
%      MagickBooleanType IsJ2K(const unsigned char *magick,const size_t length)
%
%  A description of each parameter follows:
%
%    o magick: compare image format pattern against these bytes.
%
%    o length: Specifies the length of the magick string.
%
*/
static MagickBooleanType IsJ2K(const unsigned char *magick,const size_t length)
{
  if (length < 4)
    return(MagickFalse);
  if (memcmp(magick,"\xff\x4f\xff\x51",4) == 0)
    return(MagickTrue);
  return(MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   I s J P 2                                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  IsJP2() returns MagickTrue if the image format type, identified by the
%  magick string, is JP2.
%
%  The format of the IsJP2 method is:
%
%      MagickBooleanType IsJP2(const unsigned char *magick,const size_t length)
%
%  A description of each parameter follows:
%
%    o magick: compare image format pattern against these bytes.
%
%    o length: Specifies the length of the magick string.
%
*/
static MagickBooleanType IsJP2(const unsigned char *magick,const size_t length)
{
  if (length < 4)
    return(MagickFalse);
  if (memcmp(magick,"\x0d\x0a\x87\x0a",4) == 0)
    return(MagickTrue);
  if (length < 12)
    return(MagickFalse);
  if (memcmp(magick,"\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a",12) == 0)
    return(MagickTrue);
  return(MagickFalse);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e a d J P 2 I m a g e                                                   %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  ReadJP2Image() reads a JPEG 2000 Image file (JP2) or JPEG 2000
%  codestream (JPC) image file and returns it.  It allocates the memory
%  necessary for the new Image structure and returns a pointer to the new
%  image or set of images.
%
%  JP2 support is originally written by Nathan Brown, nathanbrown@letu.edu.
%
%  The format of the ReadJP2Image method is:
%
%      Image *ReadJP2Image(const ImageInfo *image_info,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows:
%
%    o image_info: the image info.
%
%    o exception: return any errors or warnings in this structure.
%
*/
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
static void JP2ErrorHandler(const char *message,void *client_data)
{
  ExceptionInfo
    *exception;

  exception=(ExceptionInfo *) client_data;
  (void) ThrowMagickException(exception,GetMagickModule(),CoderError,
    message,"`%s'","OpenJP2");
}

static OPJ_SIZE_T JP2ReadHandler(void *buffer,OPJ_SIZE_T length,void *context)
{
  Image
    *image;

  ssize_t
    count;

  image=(Image *) context;
  count=ReadBlob(image,(size_t) length,(unsigned char *) buffer);
  if (count == 0)
    return((OPJ_SIZE_T) -1);
  return((OPJ_SIZE_T) count);
}

static OPJ_BOOL JP2SeekHandler(OPJ_OFF_T offset,void *context)
{
  Image
    *image;

  image=(Image *) context;
  return(SeekBlob(image,offset,SEEK_SET) < 0 ? OPJ_FALSE : OPJ_TRUE);
}

static OPJ_OFF_T JP2SkipHandler(OPJ_OFF_T offset,void *context)
{
  Image
    *image;

  image=(Image *) context;
  return(SeekBlob(image,offset,SEEK_CUR) < 0 ? -1 : offset);
}

static void JP2WarningHandler(const char *message,void *client_data)
{
  ExceptionInfo
    *exception;

  exception=(ExceptionInfo *) client_data;
  (void) ThrowMagickException(exception,GetMagickModule(),CoderWarning,
    message,"`%s'","OpenJP2");
}

static OPJ_SIZE_T JP2WriteHandler(void *buffer,OPJ_SIZE_T length,void *context)
{
  Image
    *image;

  ssize_t
    count;

  image=(Image *) context;
  count=WriteBlob(image,(size_t) length,(unsigned char *) buffer);
  return((OPJ_SIZE_T) count);
}

static MagickBooleanType JP2ComponentHasAlpha(const ImageInfo* image_info,
  opj_image_comp_t comp)
{
  const char
    *option;

  if (comp.alpha != 0)
    return(MagickTrue);
  option=GetImageOption(image_info, "jp2:assume-alpha");
  return(IsStringTrue(option));
}

static Image *ReadJP2Image(const ImageInfo *image_info,ExceptionInfo *exception)
{
  const char
    *option;

  Image
    *image;

  int
    jp2_status;

  JP2CompsInfo
    comps_info[MaxPixelChannels];

  MagickBooleanType
    status;

  opj_codec_t
    *jp2_codec;

  opj_codestream_info_v2_t
    *jp2_codestream_info;

  opj_dparameters_t
    parameters;

  opj_image_t
    *jp2_image;

  opj_stream_t
    *jp2_stream;

  ssize_t
    i,
    y;

  unsigned char
    magick[16];

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",
      image_info->filename);
  image=AcquireImage(image_info,exception);
  status=OpenBlob(image_info,image,ReadBinaryBlobMode,exception);
  if (status == MagickFalse)
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  /*
    Initialize JP2 codec.
  */
  if (ReadBlob(image,sizeof(magick),magick) != sizeof(magick))
    {
      image=DestroyImageList(image);
      return((Image *) NULL);
    }
  (void) SeekBlob(image,SEEK_SET,0);
  if (LocaleCompare(image_info->magick,"JPT") == 0)
    jp2_codec=opj_create_decompress(OPJ_CODEC_JPT);
  else
    if (IsJ2K(magick,sizeof(magick)) != MagickFalse)
      jp2_codec=opj_create_decompress(OPJ_CODEC_J2K);
    else
      if (IsJP2(magick,sizeof(magick)) != MagickFalse)
        jp2_codec=opj_create_decompress(OPJ_CODEC_JP2);
      else
        ThrowReaderException(DelegateError,"UnableToManageJP2Stream");
  opj_set_warning_handler(jp2_codec,JP2WarningHandler,exception);
  opj_set_error_handler(jp2_codec,JP2ErrorHandler,exception);
  opj_set_default_decoder_parameters(&parameters);
  option=GetImageOption(image_info,"jp2:reduce-factor");
  if (option != (const char *) NULL)
    parameters.cp_reduce=(unsigned int) StringToInteger(option);
  option=GetImageOption(image_info,"jp2:quality-layers");
  if (option != (const char *) NULL)
    parameters.cp_layer=(unsigned int) StringToInteger(option);
  if (opj_setup_decoder(jp2_codec,&parameters) == 0)
    {
      opj_destroy_codec(jp2_codec);
      ThrowReaderException(DelegateError,"UnableToManageJP2Stream");
    }
  jp2_stream=opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE,1);
  opj_stream_set_read_function(jp2_stream,JP2ReadHandler);
  opj_stream_set_write_function(jp2_stream,JP2WriteHandler);
  opj_stream_set_seek_function(jp2_stream,JP2SeekHandler);
  opj_stream_set_skip_function(jp2_stream,JP2SkipHandler);
  opj_stream_set_user_data(jp2_stream,image,NULL);
  opj_stream_set_user_data_length(jp2_stream,GetBlobSize(image));
  jp2_image=(opj_image_t *) NULL;
  if (opj_read_header(jp2_stream,jp2_codec,&jp2_image) == 0)
    {
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      ThrowReaderException(DelegateError,"UnableToDecodeImageFile");
    }
  jp2_codestream_info=opj_get_cstr_info(jp2_codec);
  if (AcquireMagickResource(ListLengthResource,(MagickSizeType)
       jp2_codestream_info->m_default_tile_info.numlayers) == MagickFalse)
    {
      opj_destroy_cstr_info(&jp2_codestream_info);
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      ThrowReaderException(ResourceLimitError,"ListLengthExceedsLimit");
    }
  opj_destroy_cstr_info(&jp2_codestream_info);
  jp2_status=OPJ_TRUE;
  if ((AcquireMagickResource(WidthResource,(size_t) jp2_image->comps[0].w) == MagickFalse) ||
      (AcquireMagickResource(WidthResource,(size_t) jp2_image->x1) == MagickFalse) ||
      (AcquireMagickResource(HeightResource,(size_t) jp2_image->comps[0].h) == MagickFalse) ||
      (AcquireMagickResource(HeightResource,(size_t) jp2_image->y1) == MagickFalse))
    {
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      ThrowReaderException(DelegateError,"UnableToDecodeImageFile");
    }
  if (image->ping == MagickFalse)
    {
      if ((image->columns != 0) && (image->rows != 0))
        /*
          Extract an area from the image.
        */
        jp2_status=opj_set_decode_area(jp2_codec,jp2_image,
          (OPJ_INT32) image->extract_info.x,(OPJ_INT32) image->extract_info.y,
          (OPJ_INT32) (image->extract_info.x+(ssize_t) image->columns),
          (OPJ_INT32) (image->extract_info.y+(ssize_t) image->rows));
      else
        jp2_status=opj_set_decode_area(jp2_codec,jp2_image,0,0,
          (int) jp2_image->comps[0].w,(int) jp2_image->comps[0].h);
      if (jp2_status == OPJ_FALSE)
        {
          opj_stream_destroy(jp2_stream);
          opj_destroy_codec(jp2_codec);
          opj_image_destroy(jp2_image);
          ThrowReaderException(DelegateError,"UnableToDecodeImageFile");
        }
    }
  if ((image_info->number_scenes != 0) && (image_info->scene != 0))
    jp2_status=opj_get_decoded_tile(jp2_codec,jp2_stream,jp2_image,
      (unsigned int) image_info->scene-1);
  else
    if (image->ping == MagickFalse)
      {
        jp2_status=opj_decode(jp2_codec,jp2_stream,jp2_image);
        if (jp2_status != OPJ_FALSE)
          jp2_status=opj_end_decompress(jp2_codec,jp2_stream);
      }
  if (jp2_status == OPJ_FALSE)
    {
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      ThrowReaderException(DelegateError,"UnableToDecodeImageFile");
    }
  if (jp2_image->numcomps >= MaxPixelChannels)
    {
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      ThrowReaderException(CorruptImageError,"ImproperImageHeader");
    }
  for (i=0; i < (ssize_t) jp2_image->numcomps; i++)
  {
    if ((jp2_image->comps[i].dx == 0) || (jp2_image->comps[i].dy == 0) ||
        (jp2_image->comps[0].prec != jp2_image->comps[i].prec) ||
        (jp2_image->comps[0].prec > 64) ||
        (jp2_image->comps[0].sgnd != jp2_image->comps[i].sgnd))
      {
        opj_stream_destroy(jp2_stream);
        opj_destroy_codec(jp2_codec);
        opj_image_destroy(jp2_image);
        ThrowReaderException(CoderError,"IrregularChannelGeometryNotSupported")
      }
  }
  opj_stream_destroy(jp2_stream);
  if (image->ping == MagickFalse)
    {
      for (i=0; i < (ssize_t) jp2_image->numcomps; i++)
        if (jp2_image->comps[i].data == NULL)
          {
            opj_destroy_codec(jp2_codec);
            opj_image_destroy(jp2_image);
            ThrowReaderException(CoderError,
              "IrregularChannelGeometryNotSupported")
          }
    }
  /*
    Convert JP2 image.
  */
  image->columns=(size_t) jp2_image->comps[0].w;
  image->rows=(size_t) jp2_image->comps[0].h;
  image->depth=jp2_image->comps[0].prec;
  image->compression=JPEG2000Compression;
  if (jp2_image->numcomps == 1)
    SetImageColorspace(image,GRAYColorspace,exception);
  else
    if (jp2_image->color_space == 2)
      SetImageColorspace(image,GRAYColorspace,exception);
    else
      if (jp2_image->color_space == 3)
        SetImageColorspace(image,Rec601YCbCrColorspace,exception);
  if (jp2_image->numcomps > 3)
    {
      size_t
        number_meta_channels;

      number_meta_channels=jp2_image->numcomps-3;
      if (JP2ComponentHasAlpha(image_info,jp2_image->comps[3]) != MagickFalse)
        {
          image->alpha_trait=BlendPixelTrait;
          number_meta_channels-=1;
        }
      if (number_meta_channels > 0)
        {
          status=SetPixelMetaChannels(image,(size_t) number_meta_channels,
            exception);
          if (status == MagickFalse)
            {
              opj_destroy_codec(jp2_codec);
              opj_image_destroy(jp2_image);
              return(DestroyImageList(image));
            }
        }
    }
  else if (jp2_image->numcomps == 2)
    {
      if (JP2ComponentHasAlpha(image_info,jp2_image->comps[1]) != MagickFalse)
        image->alpha_trait=BlendPixelTrait;
    }
  else if ((jp2_image->numcomps == 1) && (jp2_image->comps[0].alpha != 0))
    image->alpha_trait=BlendPixelTrait;
  if (jp2_image->icc_profile_buf != (unsigned char *) NULL)
    {
      StringInfo
        *profile;

      profile=BlobToProfileStringInfo("icc",jp2_image->icc_profile_buf,
        jp2_image->icc_profile_len,exception);
      (void) SetImageProfilePrivate(image,profile,exception);
    }
  if (image->ping != MagickFalse)
    {
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      return(GetFirstImageInList(image));
    }
  status=SetImageExtent(image,image->columns,image->rows,exception);
  if (status == MagickFalse)
    {
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      return(DestroyImageList(image));
    }
  memset(comps_info,0,MaxPixelChannels*sizeof(JP2CompsInfo));
  for (i=0; i < (ssize_t) jp2_image->numcomps; i++)
  {
    comps_info[i].scale=(double) QuantumRange/(double) 
      ((MagickULLConstant(1) << jp2_image->comps[i].prec)-1);
    comps_info[i].addition=(jp2_image->comps[i].sgnd ?
      MagickULLConstant(1) << (jp2_image->comps[i].prec-1) : 0);
    comps_info[i].pad=(ssize_t) image->columns % jp2_image->comps[i].dx;
  }
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    Quantum
      *magick_restrict q;

    ssize_t
      x;

    for (i=0; i < (ssize_t) jp2_image->numcomps; i++)
    {
      comps_info[i].y_index=y/jp2_image->comps[i].dy*((ssize_t) image->columns+
        comps_info[i].pad);
    }
    q=GetAuthenticPixels(image,0,y,image->columns,1,exception);
    if (q == (Quantum *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      for (i=0; i < (ssize_t) jp2_image->numcomps; i++)
      {
        double
          pixel;

        ssize_t
          index;

        index=comps_info[i].y_index/jp2_image->comps[i].dx+x/
          jp2_image->comps[i].dx;
        if ((index < 0) ||
            (index >= (ssize_t) (jp2_image->comps[i].h*jp2_image->comps[i].w)))
          {
            opj_destroy_codec(jp2_codec);
            opj_image_destroy(jp2_image);
            ThrowReaderException(CoderError,
              "IrregularChannelGeometryNotSupported")
          }
        pixel=comps_info[i].scale*(jp2_image->comps[i].data[index]+
          comps_info[i].addition);
        switch (i)
        {
          case 0:
          {
            if (jp2_image->numcomps == 1)
              {
                SetPixelGray(image,ClampToQuantum(pixel),q);
                if (jp2_image->comps[i].alpha != 0)
                  SetPixelAlpha(image,ClampToQuantum(pixel),q);
                break;
              }
            SetPixelRed(image,ClampToQuantum(pixel),q);
            if (jp2_image->numcomps == 2)
              {
                SetPixelGreen(image,ClampToQuantum(pixel),q);
                SetPixelBlue(image,ClampToQuantum(pixel),q);
              }
            break;
          }
          case 1:
          {
            if ((jp2_image->numcomps == 2) && (jp2_image->comps[i].alpha != 0))
              {
                SetPixelAlpha(image,ClampToQuantum(pixel),q);
                break;
              }
            SetPixelGreen(image,ClampToQuantum(pixel),q);
            break;
          }
          case 2:
          {
            SetPixelBlue(image,ClampToQuantum(pixel),q);
            break;
          }
          case 3:
          {
            if ((image->alpha_trait & BlendPixelTrait) != 0)
              SetPixelAlpha(image,ClampToQuantum(pixel),q);
            else
              SetPixelChannel(image,MetaPixelChannels,ClampToQuantum(pixel),q);
            break;
          }
          default:
          {
            ssize_t
              offset = MetaPixelChannels+i-3;

            if ((image->alpha_trait & BlendPixelTrait) != 0)
              offset--;
            SetPixelChannel(image,(PixelChannel) offset,ClampToQuantum(pixel),
              q);
            break;
          }
        }
      }
      q+=(ptrdiff_t) GetPixelChannels(image);
    }
    if (SyncAuthenticPixels(image,exception) == MagickFalse)
      break;
    status=SetImageProgress(image,LoadImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  /*
    Free resources.
  */
  opj_destroy_codec(jp2_codec);
  opj_image_destroy(jp2_image);
  (void) CloseBlob(image);
  if ((image_info->number_scenes != 0) && (image_info->scene != 0))
    AppendImageToList(&image,CloneImage(image,0,0,MagickTrue,exception));
  return(GetFirstImageInList(image));
}
#endif

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   R e g i s t e r J P 2 I m a g e                                           %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  RegisterJP2Image() adds attributes for the JP2 image format to the list of
%  supported formats.  The attributes include the image format tag, a method
%  method to read and/or write the format, whether the format supports the
%  saving of more than one frame to the same file or blob, whether the format
%  supports native in-memory I/O, and a brief description of the format.
%
%  The format of the RegisterJP2Image method is:
%
%      size_t RegisterJP2Image(void)
%
*/
ModuleExport size_t RegisterJP2Image(void)
{
  char
    version[MagickPathExtent];

  MagickInfo
    *entry;

  *version='\0';
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  (void) FormatLocaleString(version,MagickPathExtent,"%s",opj_version());
#endif
  entry=AcquireMagickInfo("JP2","JP2","JPEG-2000 File Format Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJP2;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("JP2","J2C","JPEG-2000 Code Stream Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJ2K;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("JP2","J2K","JPEG-2000 Code Stream Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJ2K;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("JP2","JPM","JPEG-2000 File Format Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJP2;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("JP2","JPT","JPEG-2000 File Format Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJP2;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  entry=AcquireMagickInfo("JP2","JPC","JPEG-2000 Code Stream Syntax");
  if (*version != '\0')
    entry->version=ConstantString(version);
  entry->mime_type=ConstantString("image/jp2");
  entry->magick=(IsImageFormatHandler *) IsJP2;
  entry->flags^=CoderAdjoinFlag;
  entry->flags|=CoderDecoderSeekableStreamFlag;
  entry->flags|=CoderEncoderSeekableStreamFlag;
#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
  entry->decoder=(DecodeImageHandler *) ReadJP2Image;
  entry->encoder=(EncodeImageHandler *) WriteJP2Image;
#endif
  (void) RegisterMagickInfo(entry);
  return(MagickImageCoderSignature);
}

/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   U n r e g i s t e r J P 2 I m a g e                                       %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  UnregisterJP2Image() removes format registrations made by the JP2 module
%  from the list of supported formats.
%
%  The format of the UnregisterJP2Image method is:
%
%      UnregisterJP2Image(void)
%
*/
ModuleExport void UnregisterJP2Image(void)
{
  (void) UnregisterMagickInfo("JPC");
  (void) UnregisterMagickInfo("JPT");
  (void) UnregisterMagickInfo("JPM");
  (void) UnregisterMagickInfo("JP2");
  (void) UnregisterMagickInfo("J2K");
}

#if defined(MAGICKCORE_LIBOPENJP2_DELEGATE)
/*
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%                                                                             %
%                                                                             %
%                                                                             %
%   W r i t e J P 2 I m a g e                                                 %
%                                                                             %
%                                                                             %
%                                                                             %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%
%  WriteJP2Image() writes an image in the JPEG 2000 image format.
%
%  JP2 support originally written by Nathan Brown, nathanbrown@letu.edu
%
%  The format of the WriteJP2Image method is:
%
%      MagickBooleanType WriteJP2Image(const ImageInfo *image_info,Image *image,
%        ExceptionInfo *exception)
%
%  A description of each parameter follows.
%
%    o image_info: the image info.
%
%    o image:  The image.
%
*/

static void CinemaProfileCompliance(const opj_image_t *jp2_image,
  opj_cparameters_t *parameters)
{
  /*
    Digital Cinema 4K profile compliant codestream.
  */
  parameters->tile_size_on=OPJ_FALSE;
  parameters->cp_tdx=1;
  parameters->cp_tdy=1;
  parameters->tp_flag='C';
  parameters->tp_on=1;
  parameters->cp_tx0=0;
  parameters->cp_ty0=0;
  parameters->image_offset_x0=0;
  parameters->image_offset_y0=0;
  parameters->cblockw_init=32;
  parameters->cblockh_init=32;
  parameters->csty|=0x01;
  parameters->prog_order=OPJ_CPRL;
  parameters->roi_compno=(-1);
  parameters->subsampling_dx=1;
  parameters->subsampling_dy=1;
  parameters->irreversible=1;
  if ((jp2_image->comps[0].w == 2048) || (jp2_image->comps[0].h == 1080))
    {
      /*
        Digital Cinema 2K.
      */
#if OPJ_NUMERIC_VERSION >= OPJ_COMPUTE_NUMERIC_VERSION(2,1,0)
      parameters->rsiz=OPJ_PROFILE_CINEMA_2K;
      parameters->max_cs_size=OPJ_CINEMA_24_CS;
      parameters->max_comp_size=OPJ_CINEMA_24_COMP;
#else
      parameters->cp_cinema=OPJ_CINEMA2K_24;
      parameters->cp_rsiz=OPJ_CINEMA2K;
      parameters->max_comp_size=1041666;
#endif
      if (parameters->numresolution > 6)
        parameters->numresolution=6;

    }
  if ((jp2_image->comps[0].w == 4096) || (jp2_image->comps[0].h == 2160))
    {
      /*
        Digital Cinema 4K.
      */
#if OPJ_NUMERIC_VERSION >= OPJ_COMPUTE_NUMERIC_VERSION(2,1,0)
      parameters->rsiz=OPJ_PROFILE_CINEMA_4K;
      parameters->max_cs_size=OPJ_CINEMA_24_CS;
      parameters->max_comp_size=OPJ_CINEMA_24_COMP;
#else
      parameters->cp_cinema=OPJ_CINEMA4K_24;
      parameters->cp_rsiz=OPJ_CINEMA4K;
      parameters->max_comp_size=1041666;
#endif
      if (parameters->numresolution < 1)
        parameters->numresolution=1;
      if (parameters->numresolution > 7)
        parameters->numresolution=7;
      parameters->numpocs=2;
      parameters->POC[0].tile=1;
      parameters->POC[0].resno0=0;
      parameters->POC[0].compno0=0;
      parameters->POC[0].layno1=1;
      parameters->POC[0].resno1=(unsigned int) parameters->numresolution-1;
      parameters->POC[0].compno1=3;
      parameters->POC[0].prg1=OPJ_CPRL;
      parameters->POC[1].tile=1;
      parameters->POC[1].resno0=(unsigned int) parameters->numresolution-1;
      parameters->POC[1].compno0=0;
      parameters->POC[1].layno1=1;
      parameters->POC[1].resno1=(unsigned int) parameters->numresolution;
      parameters->POC[1].compno1=3;
      parameters->POC[1].prg1=OPJ_CPRL;
    }
  parameters->tcp_numlayers=1;
  parameters->tcp_rates[0]=(((float) jp2_image->numcomps*jp2_image->comps[0].w*
    jp2_image->comps[0].h*jp2_image->comps[0].prec))/((float)
    parameters->max_comp_size*8*jp2_image->comps[0].dx*jp2_image->comps[0].dy);
  parameters->cp_disto_alloc=OPJ_TRUE;
}

static inline int CalculateNumResolutions(size_t width,size_t height)
{
  int
    i;

  for (i=1; i < 6; i++)
    if ((width < ((size_t) MagickULLConstant(1) << i)) ||
        (height < ((size_t) MagickULLConstant(1) << i)))
      break;
  return(i);
}

static MagickBooleanType WriteJP2Image(const ImageInfo *image_info,Image *image,
  ExceptionInfo *exception)
{
  const char
    *option,
    *property;

  int
    jp2_status;

  MagickBooleanType
    status;

  opj_codec_t
    *jp2_codec;

  OPJ_COLOR_SPACE
    jp2_colorspace;

  opj_cparameters_t
    *parameters;

  opj_image_cmptparm_t
    jp2_info[5];

  opj_image_t
    *jp2_image;

  opj_stream_t
    *jp2_stream;

  ssize_t
    i,
    y;

  unsigned int
    channels;

  /*
    Open image file.
  */
  assert(image_info != (const ImageInfo *) NULL);
  assert(image_info->signature == MagickCoreSignature);
  assert(image != (Image *) NULL);
  assert(image->signature == MagickCoreSignature);
  assert(exception != (ExceptionInfo *) NULL);
  assert(exception->signature == MagickCoreSignature);
  if (IsEventLogging() != MagickFalse)
    (void) LogMagickEvent(TraceEvent,GetMagickModule(),"%s",image->filename);
  status=OpenBlob(image_info,image,WriteBinaryBlobMode,exception);
  if (status == MagickFalse)
    return(status);
  /*
    Initialize JPEG 2000 API.
  */
  parameters=(opj_cparameters_t *) AcquireMagickMemory(sizeof(*parameters));
  if (parameters == (opj_cparameters_t *) NULL)
    ThrowWriterException(ResourceLimitError,"MemoryAllocationFailed");
  opj_set_default_encoder_parameters(parameters);
  option=GetImageOption(image_info,"jp2:number-resolutions");
  if (option != (const char *) NULL)
    parameters->numresolution=StringToInteger(option);
  else
    parameters->numresolution=CalculateNumResolutions(image->columns,
      image->rows);
  parameters->tcp_numlayers=1;
  parameters->tcp_rates[0]=0;  /* lossless */
  parameters->cp_disto_alloc=OPJ_TRUE;
  if ((image_info->quality != 0) && (image_info->quality != 100))
    {
      parameters->tcp_distoratio[0]=(float) image_info->quality;
      parameters->cp_fixed_quality=OPJ_TRUE;
      parameters->cp_disto_alloc=OPJ_FALSE;
    }
  if (image_info->extract != (char *) NULL)
    {
      int
        flags;

      RectangleInfo
        geometry;

      /*
        Set tile size.
      */
      (void) memset(&geometry,0,sizeof(geometry));
      flags=(int) ParseAbsoluteGeometry(image_info->extract,&geometry);
      parameters->cp_tdx=(int) geometry.width;
      parameters->cp_tdy=(int) geometry.width;
      if ((flags & HeightValue) != 0)
        parameters->cp_tdy=(int) geometry.height;
      if ((flags & XValue) != 0)
        parameters->cp_tx0=(int) geometry.x;
      if ((flags & YValue) != 0)
        parameters->cp_ty0=(int) geometry.y;
      parameters->tile_size_on=OPJ_TRUE;
      parameters->numresolution=CalculateNumResolutions((size_t)
        parameters->cp_tdx,(size_t) parameters->cp_tdy);
    }
  option=GetImageOption(image_info,"jp2:quality");
  if (option != (const char *) NULL)
    {
      const char
        *p;

      /*
        Set quality PSNR.
      */
      p=option;
      for (i=0; MagickSscanf(p,"%f",&parameters->tcp_distoratio[i]) == 1; i++)
      {
        if (i > 100)
          break;
        while ((*p != '\0') && (*p != ','))
          p++;
        if (*p == '\0')
          break;
        p++;
      }
      parameters->tcp_numlayers=(int) (i+1);
      parameters->cp_fixed_quality=OPJ_TRUE;
      parameters->cp_disto_alloc=OPJ_FALSE;
    }
  option=GetImageOption(image_info,"jp2:progression-order");
  if (option != (const char *) NULL)
    {
      if (LocaleCompare(option,"LRCP") == 0)
        parameters->prog_order=OPJ_LRCP;
      if (LocaleCompare(option,"RLCP") == 0)
        parameters->prog_order=OPJ_RLCP;
      if (LocaleCompare(option,"RPCL") == 0)
        parameters->prog_order=OPJ_RPCL;
      if (LocaleCompare(option,"PCRL") == 0)
        parameters->prog_order=OPJ_PCRL;
      if (LocaleCompare(option,"CPRL") == 0)
        parameters->prog_order=OPJ_CPRL;
    }
  option=GetImageOption(image_info,"jp2:rate");
  if (option != (const char *) NULL)
    {
      const char
        *p;

      /*
        Set compression rate.
      */
      p=option;
      for (i=0; MagickSscanf(p,"%f",&parameters->tcp_rates[i]) == 1; i++)
      {
        if (i >= 100)
          break;
        while ((*p != '\0') && (*p != ','))
          p++;
        if (*p == '\0')
          break;
        p++;
      }
      parameters->tcp_numlayers=(int) (i+1);
      parameters->cp_disto_alloc=OPJ_TRUE;
    }
  if (image_info->sampling_factor != (char *) NULL)
    {
      GeometryInfo
        geometry_info;

      MagickStatusType
        flags;

      flags=ParseGeometry(image_info->sampling_factor,&geometry_info);
      if ((flags & RhoValue) != 0)
        parameters->subsampling_dx=(int) geometry_info.rho;
      parameters->subsampling_dy=parameters->subsampling_dx;
      if ((flags & SigmaValue) != 0)
        parameters->subsampling_dy=(int) geometry_info.sigma;
    }   
  property=GetImageProperty(image,"comment",exception);
  if (property != (const char *) NULL)
    parameters->cp_comment=(char *) property;
  channels=3;
  jp2_colorspace=OPJ_CLRSPC_SRGB;
  if (image->colorspace == YUVColorspace)
    jp2_colorspace=OPJ_CLRSPC_SYCC;
  else
    {
      if (IsGrayColorspace(image->colorspace) != MagickFalse)
        {
          channels=1;
          jp2_colorspace=OPJ_CLRSPC_GRAY;
        }
      else
        if (IssRGBCompatibleColorspace(image->colorspace) == MagickFalse)
          (void) TransformImageColorspace(image,sRGBColorspace,exception);
      if (image->alpha_trait != UndefinedPixelTrait)
        channels++;
    }
  parameters->tcp_mct=channels == 3 ? 1 : 0;
  memset(jp2_info,0,sizeof(jp2_info));
  for (i=0; i < (ssize_t) channels; i++)
  {
    jp2_info[i].prec=(OPJ_UINT32) image->depth;
    if ((image->depth == 1) &&
        ((LocaleCompare(image_info->magick,"JPT") == 0) ||
         (LocaleCompare(image_info->magick,"JP2") == 0)))
      jp2_info[i].prec++;  /* OpenJPEG returns exception for depth @ 1 */
    jp2_info[i].sgnd=0;
    jp2_info[i].dx=(unsigned int) parameters->subsampling_dx;
    jp2_info[i].dy=(unsigned int) parameters->subsampling_dy;
    jp2_info[i].w=(OPJ_UINT32) image->columns;
    jp2_info[i].h=(OPJ_UINT32) image->rows;
  }
  jp2_image=opj_image_create((OPJ_UINT32) channels,jp2_info,jp2_colorspace);
  if (jp2_image == (opj_image_t *) NULL)
    {
      parameters=(opj_cparameters_t *) RelinquishMagickMemory(parameters);
      ThrowWriterException(DelegateError,"UnableToEncodeImageFile");
    }
  jp2_image->x0=(unsigned int) parameters->image_offset_x0;
  jp2_image->y0=(unsigned int) parameters->image_offset_y0;
  jp2_image->x1=(unsigned int) (2*parameters->image_offset_x0+
    ((ssize_t) image->columns-1)*parameters->subsampling_dx+1);
  jp2_image->y1=(unsigned int) (2*parameters->image_offset_y0+
    ((ssize_t) image->rows-1)*parameters->subsampling_dx+1);
  if ((image->depth == 12) &&
      ((image->columns == 2048) || (image->rows == 1080) ||
       (image->columns == 4096) || (image->rows == 2160)))
    CinemaProfileCompliance(jp2_image,parameters);
  if (channels == 4)
    jp2_image->comps[3].alpha=1;
  else
    if ((channels == 2) && (jp2_colorspace == OPJ_CLRSPC_GRAY))
      jp2_image->comps[1].alpha=1;
  /*
    Convert to JP2 pixels.
  */
  for (y=0; y < (ssize_t) image->rows; y++)
  {
    const Quantum
      *p;

    ssize_t
      x;

    p=GetVirtualPixels(image,0,y,image->columns,1,exception);
    if (p == (const Quantum *) NULL)
      break;
    for (x=0; x < (ssize_t) image->columns; x++)
    {
      for (i=0; i < (ssize_t) channels; i++)
      {
        double
          scale;

        int
          *q;

        scale=(double) (((size_t) MagickULLConstant(1) <<
          jp2_image->comps[i].prec)-1)/(double) QuantumRange;
        q=jp2_image->comps[i].data+(ssize_t) (y*MagickSafeReciprocal(
          jp2_image->comps[i].dy)*image->columns*MagickSafeReciprocal(
          jp2_image->comps[i].dx)+x*MagickSafeReciprocal(
          jp2_image->comps[i].dx));
        switch (i)
        {
          case 0:
          {
            if (jp2_colorspace == OPJ_CLRSPC_GRAY)
              {
                *q=(int) (scale*(double) GetPixelGray(image,p));
                break;
              }
            *q=(int) (scale*(double) GetPixelRed(image,p));
            break;
          }
          case 1:
          {
            if (jp2_colorspace == OPJ_CLRSPC_GRAY)
              {
                *q=(int) (scale*(double) GetPixelAlpha(image,p));
                break;
              }
            *q=(int) (scale*(double) GetPixelGreen(image,p));
            break;
          }
          case 2:
          {
            *q=(int) (scale*(double) GetPixelBlue(image,p));
            break;
          }
          case 3:
          {
            *q=(int) (scale*(double) GetPixelAlpha(image,p));
            break;
          }
        }
      }
      p+=(ptrdiff_t) GetPixelChannels(image);
    }
    status=SetImageProgress(image,SaveImageTag,(MagickOffsetType) y,
      image->rows);
    if (status == MagickFalse)
      break;
  }
  if (LocaleCompare(image_info->magick,"JPT") == 0)
    jp2_codec=opj_create_compress(OPJ_CODEC_JPT);
  else
    if (LocaleCompare(image_info->magick,"J2K") == 0)
      jp2_codec=opj_create_compress(OPJ_CODEC_J2K);
    else
      jp2_codec=opj_create_compress(OPJ_CODEC_JP2);
  opj_set_warning_handler(jp2_codec,JP2WarningHandler,exception);
  opj_set_error_handler(jp2_codec,JP2ErrorHandler,exception);
  opj_setup_encoder(jp2_codec,parameters,jp2_image);
  jp2_stream=opj_stream_create(OPJ_J2K_STREAM_CHUNK_SIZE,OPJ_FALSE);
  if (jp2_stream == (opj_stream_t *) NULL)
    {
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      parameters=(opj_cparameters_t *) RelinquishMagickMemory(parameters);
      ThrowWriterException(DelegateError,"UnableToEncodeImageFile");
    }
  opj_stream_set_read_function(jp2_stream,JP2ReadHandler);
  opj_stream_set_write_function(jp2_stream,JP2WriteHandler);
  opj_stream_set_seek_function(jp2_stream,JP2SeekHandler);
  opj_stream_set_skip_function(jp2_stream,JP2SkipHandler);
  opj_stream_set_user_data(jp2_stream,image,NULL);
  jp2_status=opj_start_compress(jp2_codec,jp2_image,jp2_stream);
  if ((jp2_status == 0) || (opj_encode(jp2_codec,jp2_stream) == 0) ||
      (opj_end_compress(jp2_codec,jp2_stream) == 0))
    {
      opj_stream_destroy(jp2_stream);
      opj_destroy_codec(jp2_codec);
      opj_image_destroy(jp2_image);
      parameters=(opj_cparameters_t *) RelinquishMagickMemory(parameters);
      ThrowWriterException(DelegateError,"UnableToEncodeImageFile");
    }
  /*
    Free resources.
  */
  opj_stream_destroy(jp2_stream);
  opj_destroy_codec(jp2_codec);
  opj_image_destroy(jp2_image);
  parameters=(opj_cparameters_t *) RelinquishMagickMemory(parameters);
  if (CloseBlob(image) == MagickFalse)
    status=MagickFalse;
  return(status);
}
#endif
