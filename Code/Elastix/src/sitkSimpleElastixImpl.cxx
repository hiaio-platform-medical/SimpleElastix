#ifndef __sitksimpleelastiximpl_cxx_
#define __sitksimpleelastiximpl_cxx_

#include "sitkSimpleElastix.h"
#include "sitkSimpleElastixImpl.h"
#include "sitkCastImageFilter.h"

namespace itk {
  namespace simple {

SimpleElastix::SimpleElastixImpl
::SimpleElastixImpl( void )
{
  // Register this class with SimpleITK
  this->m_DualMemberFactory.reset( new detail::DualMemberFunctionFactory< MemberFunctionType >( this ) );
  this->m_DualMemberFactory->RegisterMemberFunctions< FloatPixelIDTypeList, FloatPixelIDTypeList, 2 >();
  this->m_DualMemberFactory->RegisterMemberFunctions< FloatPixelIDTypeList, FloatPixelIDTypeList, 3 >();

#ifdef SITK_4D_IMAGES
  this->m_DualMemberFactory->RegisterMemberFunctions< FloatPixelIDTypeList, FloatPixelIDTypeList, 4 >();
#endif
 
  m_FixedImages                 = VectorOfImage();
  m_MovingImages                = VectorOfImage();
  m_FixedMasks                  = VectorOfImage();
  m_MovingMasks                 = VectorOfImage();
  m_ResultImage                 = Image();

  m_ParameterMapVector          = ParameterMapVectorType();
  m_TransformParameterMapVector = ParameterMapVectorType();

  m_FixedPointSetFileName       = "";
  m_MovingPointSetFileName      = "";

  m_OutputDirectory             = ".";
  m_LogFileName                 = "";

  this->LogToFileOff();
  this->LogToConsoleOff();

  ParameterMapVectorType defaultParameterMap;
  defaultParameterMap.push_back( ParameterObjectType::GetDefaultParameterMap( "translation" ) );
  defaultParameterMap.push_back( ParameterObjectType::GetDefaultParameterMap( "affine" ) );
  defaultParameterMap.push_back( ParameterObjectType::GetDefaultParameterMap( "bspline" ) );
  this->SetParameterMap( defaultParameterMap );
}

SimpleElastix::SimpleElastixImpl
::~SimpleElastixImpl( void )
{
}


Image
SimpleElastix::SimpleElastixImpl
::Execute( void )
{
  if( this->GetNumberOfFixedImages() == 0 )
  {
    sitkExceptionMacro( "Fixed image not set." );
  }

  if( this->GetNumberOfMovingImages() == 0 )
  {
    sitkExceptionMacro( "Moving image not set." );
  }

  const PixelIDValueEnum FixedImagePixelID = this->GetFixedImage( 0 ).GetPixelID();
  const unsigned int FixedImageDimension = this->GetFixedImage( 0 ).GetDimension();
  const PixelIDValueEnum MovingImagePixelID = this->GetMovingImage( 0 ).GetPixelID();
  const unsigned int MovingImageDimension = this->GetMovingImage( 0 ).GetDimension();

  for( unsigned int i = 1; i < this->GetNumberOfFixedImages(); ++i )
  {
    if( this->GetFixedImage( i ).GetDimension() != FixedImageDimension )
    {
      sitkExceptionMacro( "Fixed images must be of same dimension (fixed image at index 0 is of dimension " 
                       << this->GetFixedImage( 0 ).GetDimension() << ", fixed image at index " << i
                       << " is of dimension \"" << this->GetFixedImage( i ).GetDimension() << "\")." );
    }
  }

  for( unsigned int i = 1; i < this->GetNumberOfMovingImages(); ++i )
  {
    if( this->GetMovingImage( i ).GetDimension() != MovingImageDimension )
    {
      sitkExceptionMacro( "Moving images must be of same dimension as fixed images (fixed image at index 0 is of dimension " 
                       << this->GetFixedImage( 0 ).GetDimension() << ", moving image at index " << i
                       << " is of dimension \"" << this->GetMovingImage( i ).GetDimension() << "\")." );
    }
  }

  for( unsigned int i = 1; i < this->GetNumberOfFixedMasks(); ++i )
  {
    if( this->GetFixedMask( i ).GetDimension() != FixedImageDimension )
    {
      sitkExceptionMacro( "Fixed masks must be of same dimension as fixed images (fixed images are of dimension " 
                       << this->GetFixedImage( 0 ).GetDimension() << ", fixed mask at index " << i
                       << " is of dimension \"" << this->GetFixedMask( i ).GetDimension() << "\")." );
    }
  }

  for( unsigned int i = 1; i < this->GetNumberOfMovingMasks(); ++i )
  {
    if( this->GetMovingMask( i ).GetDimension() != MovingImageDimension )
    {
      sitkExceptionMacro( "Moving masks must be of same dimension as moving images (moving images are of dimension " 
                       << this->GetMovingImage( 0 ).GetDimension() << ", moving mask at index " << i
                       << " is of dimension \"" << this->GetMovingMask( i ).GetDimension() << "\")." );
    }
  }

  for( unsigned int i = 0; i < this->GetNumberOfFixedMasks(); ++i )
  {
    if( this->GetFixedMask( i ).GetPixelID() != sitkUInt8 )
    {
      sitkExceptionMacro( "Fixed mask must be of pixel type unsigned char (fixed mask at index " 
                       << i << " is of type \"" << GetPixelIDValueAsElastixParameter( this->GetFixedMask( i ).GetPixelID() ) << "\")." );
    }
  }

  for( unsigned int i = 0; i < this->GetNumberOfMovingMasks(); ++i )
  {
    if( this->GetMovingMask( i ).GetPixelID() != sitkUInt8 )
    {
      sitkExceptionMacro( "Moving mask must be of pixel type unsigned char (moving mask at index " 
                       << i << " is of type \"" << GetPixelIDValueAsElastixParameter( this->GetMovingMask( i ).GetPixelID() ) << "\")." );
    }
  }

  if( this->m_DualMemberFactory->HasMemberFunction( sitkFloat32, sitkFloat32, FixedImageDimension ) )
  {
    return this->m_DualMemberFactory->GetMemberFunction( sitkFloat32, sitkFloat32, FixedImageDimension )();
  }

  sitkExceptionMacro( << "SimpleElastix does not support the combination of "
                      << FixedImageDimension << "-dimensional "
                      << GetPixelIDValueAsElastixParameter( FixedImagePixelID ) << " fixed image and a "
                      << MovingImageDimension << "-dimensional " 
                      << GetPixelIDValueAsElastixParameter( MovingImagePixelID ) << " moving image. "
                      << "This a serious error. Contact developers at https://github.com/kaspermarstal/SimpleElastix/issues." )
}

template< typename TFixedImage, typename TMovingImage >
Image
SimpleElastix::SimpleElastixImpl
::DualExecuteInternal( void )
{
  typedef elastix::ElastixFilter< TFixedImage, TMovingImage >   ElastixFilterType;
  typedef typename ElastixFilterType::Pointer                   ElastixFilterPointer;
  typedef typename ElastixFilterType::FixedMaskType             FixedMaskType;
  typedef typename ElastixFilterType::MovingMaskType            MovingMaskType;

  try
  {
    ElastixFilterPointer elastixFilter = ElastixFilterType::New();

    for( unsigned int i = 0; i < this->GetNumberOfFixedImages(); ++i )
    {
      elastixFilter->AddFixedImage( itkDynamicCastInDebugMode< TFixedImage* >( Cast( this->GetFixedImage( i ), sitkFloat32 ).GetITKBase() ) );
    }

    for( unsigned int i = 0; i < this->GetNumberOfMovingImages(); ++i )
    {
      elastixFilter->AddMovingImage( itkDynamicCastInDebugMode< TMovingImage* >( Cast( this->GetMovingImage( i ), sitkFloat32 ).GetITKBase() ) );
    }

    for( unsigned int i = 0; i < this->GetNumberOfFixedMasks(); ++i )
    {
      elastixFilter->AddFixedMask( itkDynamicCastInDebugMode< FixedMaskType* >( this->GetFixedMask( i ).GetITKBase() ) );
    }

    for( unsigned int i = 0; i < this->GetNumberOfMovingMasks(); ++i )
    {
      elastixFilter->AddMovingMask( itkDynamicCastInDebugMode< MovingMaskType* >( this->GetMovingMask( i ).GetITKBase() ) );
    }

    elastixFilter->SetInitialTransformParameterFileName( this->GetInitialTransformParameterFileName() );
    elastixFilter->SetFixedPointSetFileName( this->GetFixedPointSetFileName() );
    elastixFilter->SetMovingPointSetFileName( this->GetMovingPointSetFileName() );

    elastixFilter->SetOutputDirectory( this->GetOutputDirectory() );
    elastixFilter->SetLogFileName( this->GetLogFileName() );
    elastixFilter->SetLogToFile( this->GetLogToFile() );
    elastixFilter->SetLogToConsole( this->GetLogToConsole() );

    ParameterMapVectorType parameterMapVector = this->m_ParameterMapVector;
    for( unsigned int i = 0; i < parameterMapVector.size(); i++ )
    {
      parameterMapVector[ i ][ "FixedInternalImagePixelType" ] 
        = ParameterValueVectorType( 1, "float" );
      parameterMapVector[ i ][ "MovingInternalImagePixelType" ]
        = ParameterValueVectorType( 1, "float" );
    }

    ParameterObjectPointer parameterObject = ParameterObjectType::New();
    parameterObject->SetParameterMap( parameterMapVector );
    elastixFilter->SetParameterObject( parameterObject );
    
    elastixFilter->Update();

    this->m_ResultImage = Image( itkDynamicCastInDebugMode< TFixedImage* >( elastixFilter->GetOutput() ) );
    this->m_ResultImage.MakeUnique();
    this->m_TransformParameterMapVector = elastixFilter->GetTransformParameterObject()->GetParameterMap();
  }
  catch( itk::ExceptionObject &e )
  {
    sitkExceptionMacro( << e );
  }

  return this->m_ResultImage;
}

const std::string 
SimpleElastix::SimpleElastixImpl
::GetName( void )
{ 
  const std::string name = "SimpleElastix";
  return name;
}

void
SimpleElastix::SimpleElastixImpl
::SetFixedImage( const Image& fixedImage )
{
  if( this->IsEmpty( fixedImage ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->RemoveFixedImage();
  this->m_FixedImages.push_back( fixedImage );
}

void
SimpleElastix::SimpleElastixImpl
::SetFixedImage( const VectorOfImage& fixedImages )
{
  if( fixedImages.size() == 0u )
  {
    sitkExceptionMacro( "Cannot set fixed images from empty vector" );
  }

  this->RemoveFixedImage();
  this->m_FixedImages = fixedImages;
}

void
SimpleElastix::SimpleElastixImpl
::AddFixedImage( const Image& fixedImage )
{
  if( this->IsEmpty( fixedImage ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->m_FixedImages.push_back( fixedImage );
}

Image&
SimpleElastix::SimpleElastixImpl
::GetFixedImage( const unsigned long index )
{
  if( index < this->m_FixedImages.size() )
  {
    return this->m_FixedImages[ index ];
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of fixed images: " << this->m_FixedImages.size() << ")" );
}

SimpleElastix::SimpleElastixImpl::VectorOfImage&
SimpleElastix::SimpleElastixImpl
::GetFixedImage( void )
{
  return this->m_FixedImages;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveFixedImage( const unsigned long index )
{
  if( index < this->m_FixedImages.size() )
  {
    this->m_FixedImages.erase( this->m_FixedImages.begin() + index );
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of fixed images: " << this->m_FixedImages.size() << ")" );
}

void
SimpleElastix::SimpleElastixImpl
::RemoveFixedImage( void )
{
  this->m_FixedImages.clear();
}

unsigned int
SimpleElastix::SimpleElastixImpl
::GetNumberOfFixedImages( void )
{
  return this->m_FixedImages.size();
}

void
SimpleElastix::SimpleElastixImpl
::SetMovingImage( const Image& movingImage )
{
  if( this->IsEmpty( movingImage ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->RemoveMovingImage();
  this->m_MovingImages.push_back( movingImage );
}

void
SimpleElastix::SimpleElastixImpl
::SetMovingImage( const VectorOfImage& movingImages )
{
  if( movingImages.size() == 0u )
  {
    sitkExceptionMacro( "Cannot set moving images from empty vector" );
  }

  this->RemoveMovingImage();
  this->m_MovingImages = movingImages;
}

void
SimpleElastix::SimpleElastixImpl
::AddMovingImage( const Image& movingImage )
{
  if( this->IsEmpty( movingImage ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->m_MovingImages.push_back( movingImage );
}

Image&
SimpleElastix::SimpleElastixImpl
::GetMovingImage( const unsigned long index )
{
  if( index < this->m_MovingImages.size() )
  {
    return this->m_MovingImages[ index ];
  }
  
  sitkExceptionMacro( "Index out of range (index: " << index << ", number of moving images: " << this->m_MovingImages.size() << ")" );
}

SimpleElastix::SimpleElastixImpl::VectorOfImage&
SimpleElastix::SimpleElastixImpl
::GetMovingImage( void )
{
  return this->m_MovingImages;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveMovingImage( const unsigned long index )
{
  if( index < this->m_MovingImages.size() )
  {
    this->m_MovingImages.erase( this->m_MovingImages.begin() + index );
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of moving images: " << this->m_MovingImages.size() << ")" );
}

void
SimpleElastix::SimpleElastixImpl
::RemoveMovingImage( void )
{
  this->m_MovingImages.clear();
}

unsigned int
SimpleElastix::SimpleElastixImpl
::GetNumberOfMovingImages( void )
{
  return this->m_MovingImages.size();
}

void
SimpleElastix::SimpleElastixImpl
::SetFixedMask( const Image& fixedMask )
{
  if( this->IsEmpty( fixedMask ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->RemoveFixedMask();
  this->m_FixedMasks.push_back( fixedMask );
}

void
SimpleElastix::SimpleElastixImpl
::SetFixedMask( const VectorOfImage& fixedMasks )
{
  if( fixedMasks.size() == 0u )
  {
    sitkExceptionMacro( "Cannot set fixed images from empty vector" );
  }

  this->RemoveFixedMask();
  this->m_FixedMasks = fixedMasks;
}

void
SimpleElastix::SimpleElastixImpl
::AddFixedMask( const Image& fixedMask )
{
  if( this->IsEmpty( fixedMask ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->m_FixedMasks.push_back( fixedMask );
}

Image&
SimpleElastix::SimpleElastixImpl
::GetFixedMask( const unsigned long index )
{
  if( index < this->m_FixedMasks.size() )
  {
    return this->m_FixedMasks[ index ];
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of fixed masks: " << this->m_FixedMasks.size() << ")" );
}

SimpleElastix::VectorOfImage&
SimpleElastix::SimpleElastixImpl
::GetFixedMask( void )
{
  return this->m_FixedMasks;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveFixedMask( const unsigned long index )
{
  if( index < this->m_FixedMasks.size()  )
  {
    this->m_FixedMasks.erase( this->m_FixedMasks.begin() + index );
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of fixed masks: " << this->m_FixedMasks.size() << ")" );
}

void 
SimpleElastix::SimpleElastixImpl
::RemoveFixedMask( void )
{
  this->m_FixedMasks.clear();
}

unsigned int
SimpleElastix::SimpleElastixImpl
::GetNumberOfFixedMasks( void )
{
  return this->m_FixedMasks.size();
}

void
SimpleElastix::SimpleElastixImpl
::SetMovingMask( const Image& movingMask )
{
  if( this->IsEmpty( movingMask ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->RemoveMovingMask();
  this->m_MovingMasks.push_back( movingMask );
}

void 
SimpleElastix::SimpleElastixImpl
::SetMovingMask( const VectorOfImage& movingMasks )
{
  if( movingMasks.size() == 0u )
  {
    sitkExceptionMacro( "Cannot set moving masks from empty vector" );
  }

  this->RemoveMovingMask();
  this->m_MovingMasks = movingMasks;
}

void
SimpleElastix::SimpleElastixImpl
::AddMovingMask( const Image& movingMask )
{
  if( this->IsEmpty( movingMask ) )
  {
    sitkExceptionMacro( "Image is empty." )
  }

  this->m_MovingMasks.push_back( movingMask );
}

Image&
SimpleElastix::SimpleElastixImpl
::GetMovingMask( const unsigned long index )
{
  if( index < this->m_MovingMasks.size()  )
  {
    return this->m_MovingMasks[ index ];
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of moving masks: " << this->m_MovingMasks.size() << ")" );
}

SimpleElastix::SimpleElastixImpl::VectorOfImage&
SimpleElastix::SimpleElastixImpl
::GetMovingMask( void )
{
  return this->m_MovingMasks;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveMovingMask( const unsigned long index )
{
  if( index < this->m_MovingMasks.size()  )
  {
    this->m_MovingMasks.erase( this->m_MovingMasks.begin() + index );
  }

  sitkExceptionMacro( "Index out of range (index: " << index << ", number of moving masks: " << this->m_MovingMasks.size() << ")" );
}

void 
SimpleElastix::SimpleElastixImpl
::RemoveMovingMask( void )
{
  this->m_MovingMasks.clear();
}

unsigned int
SimpleElastix::SimpleElastixImpl
::GetNumberOfMovingMasks( void )
{
  return this->m_MovingMasks.size();
}

void
SimpleElastix::SimpleElastixImpl
::SetFixedPointSetFileName( const std::string fixedPointSetFileName )
{
  this->m_FixedPointSetFileName = fixedPointSetFileName;
}

std::string
SimpleElastix::SimpleElastixImpl
::GetFixedPointSetFileName( void )
{
  return this->m_FixedPointSetFileName;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveFixedPointSetFileName( void )
{
  this->m_FixedPointSetFileName = "";
}

void 
SimpleElastix::SimpleElastixImpl
::SetMovingPointSetFileName( const std::string movingPointSetFileName )
{
  this->m_MovingPointSetFileName = movingPointSetFileName;
}

std::string
SimpleElastix::SimpleElastixImpl
::GetMovingPointSetFileName( void )
{
  return this->m_MovingPointSetFileName;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveMovingPointSetFileName( void )
{
  this->m_MovingPointSetFileName = "";
}

void
SimpleElastix::SimpleElastixImpl
::SetOutputDirectory( const std::string outputDirectory )
{
  this->m_OutputDirectory = outputDirectory;
}

std::string
SimpleElastix::SimpleElastixImpl
::GetOutputDirectory( void )
{
  return this->m_OutputDirectory;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveOutputDirectory( void )
{
  this->m_OutputDirectory = "";
}

void
SimpleElastix::SimpleElastixImpl
::SetLogFileName( std::string logFileName )
{
  this->m_LogFileName = logFileName;
}

std::string
SimpleElastix::SimpleElastixImpl
::GetLogFileName( void )
{
  return this->m_LogFileName;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveLogFileName( void )
{
  this->m_LogFileName = "";
}

void
SimpleElastix::SimpleElastixImpl
::SetLogToFile( bool logToFile )
{
  this->m_LogToFile = logToFile;
}

bool
SimpleElastix::SimpleElastixImpl
::GetLogToFile( void )
{
  return this->m_LogToFile;
}

void
SimpleElastix::SimpleElastixImpl
::LogToFileOn()
{
  this->SetLogToFile( true );
}

void
SimpleElastix::SimpleElastixImpl
::LogToFileOff()
{
  this->SetLogToFile( false );
}

void
SimpleElastix::SimpleElastixImpl
::SetLogToConsole( bool logToConsole )
{
  this->m_LogToConsole = logToConsole;
}

bool
SimpleElastix::SimpleElastixImpl
::GetLogToConsole( void )
{
  return this->m_LogToConsole;
}

void
SimpleElastix::SimpleElastixImpl
::LogToConsoleOn()
{
  this->SetLogToConsole( true );
}

void
SimpleElastix::SimpleElastixImpl
::LogToConsoleOff()
{
  this->SetLogToConsole( false );
}

void
SimpleElastix::SimpleElastixImpl
::SetParameterMap( const std::string transformName, const unsigned int numberOfResolutions, const double finalGridSpacingInPhysicalUnits )
{
  ParameterMapType parameterMap = ParameterObjectType::GetDefaultParameterMap( transformName, numberOfResolutions, finalGridSpacingInPhysicalUnits );
  this->SetParameterMap( parameterMap );
}

void
SimpleElastix::SimpleElastixImpl
::SetParameterMap( const ParameterMapType parameterMap )
{
  ParameterMapVectorType parameterMapVector = ParameterMapVectorType( 1, parameterMap );
  this->SetParameterMap( parameterMapVector );
}

void
SimpleElastix::SimpleElastixImpl
::SetParameterMap( const ParameterMapVectorType parameterMapVector )
{
  this->m_ParameterMapVector = parameterMapVector;
}

void
SimpleElastix::SimpleElastixImpl
::AddParameterMap( const ParameterMapType parameterMap )
{
  this->m_ParameterMapVector.push_back( parameterMap );
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType 
SimpleElastix::SimpleElastixImpl
::GetParameterMap( void )
{
  return this->m_ParameterMapVector;
}

unsigned int 
SimpleElastix::SimpleElastixImpl
::GetNumberOfParameterMaps( void )
{
  return this->m_ParameterMapVector.size();
}

void
SimpleElastix::SimpleElastixImpl
::SetInitialTransformParameterFileName( const std::string initialTransformParameterFileName )
{
  this->m_InitialTransformParameterMapFileName = initialTransformParameterFileName;
}

std::string
SimpleElastix::SimpleElastixImpl
::GetInitialTransformParameterFileName( void )
{
  return m_InitialTransformParameterMapFileName ;
}

void
SimpleElastix::SimpleElastixImpl
::RemoveInitialTransformParameterFileName( void )
{
  this->m_InitialTransformParameterMapFileName = "";
}

void
SimpleElastix::SimpleElastixImpl
::SetParameter( const ParameterKeyType key, const ParameterValueType value )
{
  for( unsigned int i = 0; i < this->m_ParameterMapVector.size(); i++ )
  {
    this->SetParameter( i, key, value );
  }
}

void
SimpleElastix::SimpleElastixImpl
::SetParameter( const ParameterKeyType key, const ParameterValueVectorType value )
{
  for( unsigned int i = 0; i < this->m_ParameterMapVector.size(); i++ )
  {
    this->SetParameter( i, key, value );
  }
}

void
SimpleElastix::SimpleElastixImpl
::SetParameter( const unsigned int index, const ParameterKeyType key, const ParameterValueType value )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << "; number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  this->m_ParameterMapVector[ index ][ key ] = ParameterValueVectorType( 1, value );
}

void
SimpleElastix::SimpleElastixImpl
::SetParameter( const unsigned int index, const ParameterKeyType key, const ParameterValueVectorType value )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << ", number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  this->m_ParameterMapVector[ index ][ key ] = value;
}

void
SimpleElastix::SimpleElastixImpl
::AddParameter( const ParameterKeyType key, const ParameterValueType value )
{
  for( unsigned int i = 0; i < this->m_ParameterMapVector.size(); i++ )
  {
    this->AddParameter( i, key, value );
  }
}

void
SimpleElastix::SimpleElastixImpl
::AddParameter( const ParameterKeyType key, const ParameterValueVectorType value )
{
  for( unsigned int i = 0; i < this->m_ParameterMapVector.size(); i++ )
  {
    this->AddParameter( i, key, value );
  }
}

void
SimpleElastix::SimpleElastixImpl
::AddParameter( const unsigned int index, const ParameterKeyType key, const ParameterValueType value )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << ", number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  if( this->m_ParameterMapVector[ index ].find( key ) == this->m_ParameterMapVector[ index ].end() )
  {
    this->SetParameter( index, key, value );
  }
  else
  {
    this->m_ParameterMapVector[ index ][ key ].push_back( value );
  }
}

void
SimpleElastix::SimpleElastixImpl
::AddParameter( const unsigned int index, const ParameterKeyType key, const ParameterValueVectorType value )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << ", number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  if( this->m_ParameterMapVector[ index ].find( key ) == this->m_ParameterMapVector[ index ].end() )
  {
    this->SetParameter( index, key, value );
  }
  else
  {
    for( unsigned int i = 0; i < value.size(); i++ )
    {
      this->m_ParameterMapVector[ index ][ key ].push_back( value[ i ] );
    }
  }
}

SimpleElastix::SimpleElastixImpl::ParameterValueVectorType
SimpleElastix::SimpleElastixImpl
::GetParameter( const ParameterKeyType key )
{
  if( this->m_ParameterMapVector.size() > 0 )
  {
    sitkExceptionMacro( "An index is needed when more than one parameter map is present. Please specify the parameter map number as the first argument." );
  }

  return this->GetParameter( 0, key );
}

SimpleElastix::SimpleElastixImpl::ParameterValueVectorType
SimpleElastix::SimpleElastixImpl
::GetParameter( const unsigned int index, const ParameterKeyType key )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << ", number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  return this->m_ParameterMapVector[ index ][ key ];
}

void
SimpleElastix::SimpleElastixImpl
::RemoveParameter( const ParameterKeyType key )
{
  for( unsigned int i = 0; i < this->m_ParameterMapVector.size(); i++ )
  {
    this->RemoveParameter( i, key );
  }
}

void
SimpleElastix::SimpleElastixImpl
::RemoveParameter( const unsigned int index, const ParameterKeyType key )
{
  if( index >= this->m_ParameterMapVector.size() )
  {
    sitkExceptionMacro( "Parameter map index is out of range (index: " << index << ", number of parameters maps: " << this->m_ParameterMapVector.size() << "). Note that indexes are zero-based." );
  }

  this->m_ParameterMapVector[ index ].erase( key );
}

SimpleElastix::SimpleElastixImpl::ParameterMapType
SimpleElastix::SimpleElastixImpl
::ReadParameterFile( const std::string fileName )
{
  ParameterObjectPointer parameterObject = ParameterObjectType::New();
  parameterObject->ReadParameterFile( fileName );
  return parameterObject->GetParameterMap( 0 );
}

void
SimpleElastix::SimpleElastixImpl
::WriteParameterFile( ParameterMapType const parameterMap, const std::string parameterFileName )
{
  ParameterObjectPointer parameterObject = ParameterObjectType::New();
  parameterObject->WriteParameterFile( parameterMap, parameterFileName );
}

SimpleElastix::SimpleElastixImpl::ParameterMapType
SimpleElastix::SimpleElastixImpl
::GetDefaultParameterMap( const std::string transformName, const unsigned int numberOfResolutions, const double finalGridSpacingInPhysicalUnits )
{ 
  return ParameterObjectType::GetDefaultParameterMap( transformName, numberOfResolutions, finalGridSpacingInPhysicalUnits );
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType 
SimpleElastix::SimpleElastixImpl
::GetTransformParameterMap( void )
{
  if( this->m_TransformParameterMapVector.size() == 0 )
  {
    sitkExceptionMacro( "Number of transform parameter maps: 0. Run registration with Execute()." );
  }

  return this->m_TransformParameterMapVector;
}

SimpleElastix::SimpleElastixImpl::ParameterMapType 
SimpleElastix::SimpleElastixImpl
::GetTransformParameterMap( const unsigned int index )
{
  if( this->GetNumberOfParameterMaps() == 0 )
  {
    sitkExceptionMacro( "Number of transform parameter maps: 0. Run registration with Execute()." );
  }

  if( this->GetNumberOfParameterMaps() <= index )
  {
    sitkExceptionMacro( "Index exceeds number of transform parameter maps (index: " << index
                     << ", number of parameter maps: " << this->GetNumberOfParameterMaps() << ")." );
  }

  return this->m_TransformParameterMapVector[ index ];
}

Image
SimpleElastix::SimpleElastixImpl
::GetResultImage( void )
{
  if( this->IsEmpty( this->m_ResultImage ) )
  {
    sitkExceptionMacro( "No result image found. Run registration with Execute()." )
  }

  return this->m_ResultImage;
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType
SimpleElastix::SimpleElastixImpl
::ExecuteInverse( void )
{
  return this->ExecuteInverse( this->GetParameterMap() );
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType
SimpleElastix::SimpleElastixImpl
::ExecuteInverse( std::map< std::string, std::vector< std::string > > inverseParameterMap )
{
  return this->ExecuteInverse( ParameterMapVectorType( 1, inverseParameterMap ) );
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType
SimpleElastix::SimpleElastixImpl
::ExecuteInverse( std::vector< std::map< std::string, std::vector< std::string > > > inverseParameterMapVector )
{
  if( this->m_FixedImages.size() == 0 )
  {
    sitkExceptionMacro( "No fixed images found. Elastix needs the fixed image of the forward transformation to compute the inverse transform.")
  }

  if( this->m_MovingImages.size() == 0 )
  {
    sitkExceptionMacro( "No moving images found. Elastix needs the moving image of the forward transformation to compute the inverse transform.")
  }

  if( this->m_TransformParameterMapVector.size() == 0 )
  {
    sitkExceptionMacro( "No forward transform parameter map found. Run forward registration before computing the inverse.")
  }

  // Write forward transform parameter file to disk
  // Head of chain
  std::vector< std::string > forwardTransformParameterFileNames;
  forwardTransformParameterFileNames.push_back( this->GetOutputDirectory() + "/forwardTransformParameterFile.0.txt" );
  ParameterMapVectorType forwardTransformParameterMaps = this->m_TransformParameterMapVector;
  forwardTransformParameterMaps[ 0 ][ "InitialTransformParametersFileName" ] = ParameterValueVectorType( 1, "NoInitialTransform" );
  for( unsigned int i = 1; i < forwardTransformParameterMaps.size(); i++ )
  {
      // Chain transform parameter file
      forwardTransformParameterFileNames.push_back( this->GetOutputDirectory() + "/forwardTransformParameterFile." + ParameterObjectType::ToString( i ) + ".txt" );
      forwardTransformParameterMaps[ i ][ "InitialTransformParametersFileName" ] = ParameterValueVectorType( 1, forwardTransformParameterFileNames[ forwardTransformParameterFileNames.size()-1 ] );
  }
  ParameterObjectPointer forwardTransformParameterMapObject = ParameterObjectType::New();
  forwardTransformParameterMapObject->SetParameterMap( forwardTransformParameterMaps );
  forwardTransformParameterMapObject->WriteParameterFile( forwardTransformParameterFileNames );

  // Setup inverse transform parameter map
  for( unsigned int i = 0; i < inverseParameterMapVector.size(); i++ )
  {
    inverseParameterMapVector[ i ][ "Registration" ] = ParameterValueVectorType( 1, "MultiResolutionRegistration" );
    inverseParameterMapVector[ i ][ "Metric" ] = ParameterValueVectorType( 1, "DisplacementMagnitudePenalty" );

    // RandomSparseMask will throw an error if no mask is supplied
    if( inverseParameterMapVector[ i ][ "ImageSampler" ].size() > 0 && inverseParameterMapVector[ i ][ "ImageSampler" ][ 0 ] == "RandomSparseMask" )
    {
      inverseParameterMapVector[ i ][ "ImageSampler" ] = ParameterValueVectorType( 1, "RandomCoordinate" );
    }
  }

  // Setup inverse registration
  SimpleElastix selx;
  selx.SetInitialTransformParameterFileName( forwardTransformParameterFileNames[ 0 ] );
  selx.SetParameterMap( inverseParameterMapVector );  

  // Pass options from this SimpleElastix
  selx.SetFixedImage( this->GetFixedImage( 0 ) ); 
  selx.SetMovingImage( this->GetFixedImage( 0 ) ); // <-- The fixed image is also used as the moving image. This is not a bug.
  selx.SetOutputDirectory( this->GetOutputDirectory() );
  selx.SetLogFileName( this->GetLogFileName() );
  selx.SetLogToFile( this->GetLogToFile() );
  selx.SetLogToConsole( this->GetLogToConsole() );

  selx.Execute();

  for( unsigned int i = 0; i < forwardTransformParameterFileNames.size(); i++ )
  {
    try
    {
      std::remove( forwardTransformParameterFileNames[ i ].c_str() );
    }
    catch( ... )
    {
      std::cout << "Error removing file " << forwardTransformParameterFileNames[ i ] << ". Continuing ... " << std::endl;
    }
  }

  // TODO: Change direction/origin/spacing to match moving image

  // Unlink the first transform parameter map
  ParameterMapVectorType inverseTransformParameterMap = selx.GetTransformParameterMap();
  inverseTransformParameterMap[ 0 ][ "InitialTransformParametersFileName" ] = ParameterValueVectorType( 1, "NoInitialTransform" );
  this->m_InverseTransformParameterMapVector = inverseTransformParameterMap;
  return this->m_InverseTransformParameterMapVector;
}

SimpleElastix::SimpleElastixImpl::ParameterMapVectorType 
SimpleElastix::SimpleElastixImpl
::GetInverseTransformParameterMap( void )
{
  if( this->m_InverseTransformParameterMapVector.size() == 0 )
  {
    sitkExceptionMacro( "Number of inverse transform parameter maps: 0. Run inverse registration with ExecuteInverse()." );
  }

  return this->m_InverseTransformParameterMapVector;
}

void
SimpleElastix::SimpleElastixImpl
::PrintParameterMap( void )
{
  if( this->GetNumberOfParameterMaps() == 0 )
  {
    sitkExceptionMacro( "Cannot print parameter maps: Number of parameter maps is 0." )
  }

  this->PrintParameterMap( this->GetParameterMap() );
}

void
SimpleElastix::SimpleElastixImpl
::PrintParameterMap( const ParameterMapType parameterMap )
{
  this->PrintParameterMap( ParameterMapVectorType( 1, parameterMap ) );
}

void
SimpleElastix::SimpleElastixImpl
::PrintParameterMap( const ParameterMapVectorType parameterMapVector )
{
  ParameterObjectPointer parameterObject = ParameterObjectType::New();
  parameterObject->SetParameterMap( parameterMapVector );
  parameterObject->Print( std::cout );
}

bool
SimpleElastix::SimpleElastixImpl
::IsEmpty( const Image& image )
{
  const bool isEmpty = image.GetWidth() == 0 && image.GetHeight() == 0;
  return isEmpty;
}

} // end namespace simple
} // end namespace itk

#endif // __sitksimpleelastiximpl_cxx_