/*
  Copyright (C) 2018-2019 SKALE Labs

  This file is part of libBLS.

  libBLS is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published
  by the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  libBLS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libBLS. If not, see <https://www.gnu.org/licenses/>.

  @file unit_tests_dkg.cpp
  @author Oleh Nikolaiev
  @date 2019
*/

#include <bls/BLSPrivateKey.h>
#include <bls/BLSPrivateKeyShare.h>
#include <bls/BLSPublicKey.h>
#include <bls/BLSSigShareSet.h>
#include <bls/BLSSignature.h>
#include <dkg/dkg.h>

#include <cstdlib>
#include <ctime>
#include <map>
#include <set>

#include <libff/algebra/curves/alt_bn128/alt_bn128_pp.hpp>
#include <libff/algebra/exponentiation/exponentiation.hpp>


#define BOOST_TEST_MODULE
#ifdef EMSCRIPTEN
#define BOOST_TEST_DISABLE_ALT_STACK
#endif  // EMSCRIPTEN

#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( DkgAlgorithm )

BOOST_AUTO_TEST_CASE( PolynomialValue ) {
    libBLS::Dkg obj = libBLS::Dkg( 3, 4 );
    std::vector< libff::alt_bn128_Fr > polynomial = { libff::alt_bn128_Fr( "1" ),
        libff::alt_bn128_Fr( "0" ), libff::alt_bn128_Fr( "1" ) };

    libff::alt_bn128_Fr value = obj.PolynomialValue( polynomial, 5 );

    BOOST_REQUIRE( value == libff::alt_bn128_Fr( "26" ) );

    polynomial.clear();

    polynomial = { libff::alt_bn128_Fr( "0" ), libff::alt_bn128_Fr( "1" ),
        libff::alt_bn128_Fr( "0" ) };

    BOOST_REQUIRE_THROW( value = obj.PolynomialValue( polynomial, 5 ), std::logic_error );
}

BOOST_AUTO_TEST_CASE( verification ) {
    libBLS::Dkg obj = libBLS::Dkg( 2, 2 );

    auto polynomial_fst = obj.GeneratePolynomial();
    auto polynomial_snd = obj.GeneratePolynomial();

    std::vector< libff::alt_bn128_G2 > verification_vector_fst =
        obj.VerificationVector( polynomial_fst );
    std::vector< libff::alt_bn128_G2 > verification_vector_snd =
        obj.VerificationVector( polynomial_snd );

    libff::alt_bn128_Fr shared_by_fst_to_snd = obj.SecretKeyContribution( polynomial_snd )[1];
    libff::alt_bn128_Fr shared_by_snd_to_fst = obj.SecretKeyContribution( polynomial_fst )[0];

    BOOST_REQUIRE( obj.Verification( 0, shared_by_snd_to_fst, verification_vector_fst ) );
    BOOST_REQUIRE( obj.Verification( 1, shared_by_fst_to_snd, verification_vector_snd ) );


    // these lines show that only correctly generated by the algorithm values can be verified
    BOOST_REQUIRE(
        obj.Verification( 0, shared_by_snd_to_fst + libff::alt_bn128_Fr::random_element(),
            verification_vector_fst ) == false );
    BOOST_REQUIRE(
        obj.Verification( 1, shared_by_fst_to_snd + libff::alt_bn128_Fr::random_element(),
            verification_vector_snd ) == false );
}

std::default_random_engine rand_gen( ( unsigned int ) time( 0 ) );

BOOST_AUTO_TEST_CASE( PolySize ) {
    for ( size_t i = 0; i < 100; i++ ) {
        size_t num_all = rand_gen() % 16 + 1;
        size_t num_signed = rand_gen() % num_all + 1;
        libBLS::Dkg obj = libBLS::Dkg( num_signed, num_all );
        std::vector< libff::alt_bn128_Fr > pol = obj.GeneratePolynomial();
        BOOST_REQUIRE( pol.size() == num_signed );
        BOOST_REQUIRE( pol.at( num_signed - 1 ) != libff::alt_bn128_Fr::zero() );
    }
}

BOOST_AUTO_TEST_CASE( ZeroSecret ) {
    for ( size_t i = 0; i < 100; i++ ) {
        libBLS::Dkg dkg_obj = libBLS::Dkg( 2, 2 );

        libff::alt_bn128_Fr num1 = libff::alt_bn128_Fr::random_element();
        libff::alt_bn128_Fr num2 = -num1;
        std::vector< libff::alt_bn128_Fr > pol;
        pol.push_back( num1 );
        pol.push_back( num2 );

        BOOST_REQUIRE_THROW( dkg_obj.SecretKeyShareCreate( pol ), std::logic_error );
    }
}

libff::alt_bn128_Fq SpoilCoord( libff::alt_bn128_Fq& sign_coord ) {
    libff::alt_bn128_Fq bad_coord = sign_coord;
    size_t n_bad_bit = rand_gen() % ( bad_coord.size_in_bits() ) + 1;

    mpz_t was_coord;
    mpz_init( was_coord );
    bad_coord.as_bigint().to_mpz( was_coord );

    mpz_t mask;
    mpz_init( mask );
    mpz_set_si( mask, n_bad_bit );

    mpz_t badCoord;
    mpz_init( badCoord );
    mpz_xor( badCoord, was_coord, mask );

    bad_coord = libff::alt_bn128_Fq( badCoord );
    mpz_clears( badCoord, was_coord, mask, 0 );

    return bad_coord;
}

std::vector< libff::alt_bn128_G2 > SpoilVerifVector(
    std::vector< libff::alt_bn128_G2 >& verif_vect ) {
    size_t elem_to_spoil = rand_gen() % verif_vect.size();
    size_t bad_coord_num = rand_gen() % 6;
    std::vector< libff::alt_bn128_G2 > bad_verif_vect = verif_vect;
    switch ( bad_coord_num ) {
    case 0:
        bad_verif_vect.at( elem_to_spoil ).X.c0 = SpoilCoord( verif_vect.at( elem_to_spoil ).X.c0 );
        break;
    case 1:
        bad_verif_vect.at( elem_to_spoil ).X.c1 = SpoilCoord( verif_vect.at( elem_to_spoil ).X.c1 );
        break;
    case 2:
        bad_verif_vect.at( elem_to_spoil ).Y.c0 = SpoilCoord( verif_vect.at( elem_to_spoil ).Y.c0 );
        break;
    case 3:
        bad_verif_vect.at( elem_to_spoil ).Y.c1 = SpoilCoord( verif_vect.at( elem_to_spoil ).Y.c1 );
        break;
    case 4:
        bad_verif_vect.at( elem_to_spoil ).Z.c0 = SpoilCoord( verif_vect.at( elem_to_spoil ).Z.c0 );
        break;
    case 5:
        bad_verif_vect.at( elem_to_spoil ).Z.c1 = SpoilCoord( verif_vect.at( elem_to_spoil ).Z.c1 );
        break;
    }
    return bad_verif_vect;
}

BOOST_AUTO_TEST_CASE( Verification2 ) {
    for ( size_t i = 0; i < 100; i++ ) {
        size_t num_all = rand_gen() % 16 + 1;
        size_t num_signed = rand_gen() % num_all + 1;
        libBLS::Dkg obj = libBLS::Dkg( num_signed, num_all );
        BOOST_REQUIRE( obj.GetN() == num_all );
        BOOST_REQUIRE( obj.GetT() == num_signed );

        std::vector< libff::alt_bn128_Fr > pol = obj.GeneratePolynomial();
        std::vector< libff::alt_bn128_Fr > secret_shares = obj.SecretKeyContribution( pol );
        std::vector< libff::alt_bn128_G2 > verif_vect = obj.VerificationVector( pol );
        for ( size_t i = 0; i < num_all; i++ ) {
            BOOST_REQUIRE( obj.Verification( i, secret_shares.at( i ), verif_vect ) );
            BOOST_REQUIRE( !obj.Verification(
                i, secret_shares.at( i ) + libff::alt_bn128_Fr::one(), verif_vect ) );
            BOOST_REQUIRE(
                !obj.Verification( i, secret_shares.at( i ), SpoilVerifVector( verif_vect ) ) );
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
